/*
 *   SPDX-FileCopyrightText: 2016 Luca Giambonini <almack@chakraos.org>
 *   SPDX-FileCopyrightText: 2016 Lisa Vitolo     <shainer@chakraos.org>
 *   SPDX-FileCopyrightText: 2017 Kyle Robbertze  <krobbertze@gmail.com>
 *   SPDX-FileCopyrightText: 2017-2018 2020, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   Calamares is Free Software: see the License-Identifier above.
 *
 */

#include "LoaderQueue.h"

#include "Config.h"
#include "network/Manager.h"
#include "utils/Logger.h"
#include "utils/RAII.h"
#include "utils/Yaml.h"

#include <QMessageBox>
#include <QNetworkReply>
#include <QTimer>

/** @brief Call fetchNext() on the queue if it can
 *
 * On destruction, a new call to fetchNext() is queued, so that
 * the queue continues loading. Calling release() before the
 * destructor skips the fetchNext(), ending the queue-loading.
 *
 * Calling done(b) is a conditional release: if @p b is @c true,
 * queues a call to done() on the queue and releases it; otherwise,
 * does nothing.
 */
class FetchNextUnless
{
public:
    FetchNextUnless( LoaderQueue* q )
        : m_q( q )
    {
    }
    ~FetchNextUnless()
    {
        if ( m_q )
        {
            QMetaObject::invokeMethod( m_q, "fetchNext", Qt::QueuedConnection );
        }
    }
    void release() { m_q = nullptr; }
    void done( bool b )
    {
        if ( b )
        {
            if ( m_q )
            {
                QMetaObject::invokeMethod( m_q, "done", Qt::QueuedConnection );
            }
            release();
        }
    }

private:
    LoaderQueue* m_q = nullptr;
};

SourceItem
SourceItem::makeSourceItem( const QString& groupsUrl, const QVariantMap& configurationMap )
{
    if ( groupsUrl == QStringLiteral( "local" ) )
    {
        return SourceItem { QUrl(), configurationMap.value( "groups" ).toList() };
    }
    else
    {
        return SourceItem { QUrl { groupsUrl }, QVariantList() };
    }
}

LoaderQueue::LoaderQueue( Config* parent )
    : QObject( parent )
    , m_config( parent )
{
}

void
LoaderQueue::append( SourceItem&& i )
{
    m_queue.append( std::move( i ) );
}

void
LoaderQueue::load()
{
    QMetaObject::invokeMethod( this, "fetchNext", Qt::QueuedConnection );
}

void
LoaderQueue::fetchNext()
{
    if ( m_queue.isEmpty() )
    {
        emit done();
        QMessageBox::information(nullptr, "Loader Queue Completed", "Do not check all items, it will break things. Only select what you know you need.");
        return;
    }

    auto source = m_queue.takeFirst();
    if ( source.isLocal() )
    {
        m_config->loadGroupList( source.data );
        emit done();
    }
    else
    {
        fetch( source.url );
    }
}

void
LoaderQueue::fetch( const QUrl& url )
{
    FetchNextUnless next( this );

    if ( !url.isValid() )
    {
        m_config->setStatus( Config::Status::FailedBadConfiguration );
        cDebug() << "Invalid URL" << url;
        return;
    }

    using namespace Calamares::Network;

    cDebug() << "Options loading groups from" << url;
    QNetworkReply* reply = Manager().asynchronousGet(
        url,
        RequestOptions( RequestOptions::FakeUserAgent | RequestOptions::FollowRedirect, std::chrono::seconds( 30 ) ) );

    if ( !reply )
    {
        cDebug() << Logger::SubEntry << "Request failed immediately.";
        // If nobody sets a different status, this will remain
        m_config->setStatus( Config::Status::FailedBadConfiguration );
    }
    else
    {
        // When the network request is done, **then** we might
        // do the next item from the queue, so don't call fetchNext() now.
        next.release();
        m_reply = reply;
        connect( reply, &QNetworkReply::finished, this, &LoaderQueue::dataArrived );
    }
}

void
LoaderQueue::dataArrived()
{
    FetchNextUnless next( this );

    if ( !m_reply || !m_reply->isFinished() )
    {
        cWarning() << "Options data called too early.";
        m_config->setStatus( Config::Status::FailedInternalError );
        return;
    }

    cDebug() << "Options group data received" << m_reply->size() << "bytes from" << m_reply->url();

    cqDeleter< QNetworkReply > d { m_reply };

    // If m_required is *false* then we still say we're ready
    // even if the reply is corrupt or missing.
    if ( m_reply->error() != QNetworkReply::NoError )
    {
        cWarning() << "unable to fetch options option lists.";
        cDebug() << Logger::SubEntry << "Options reply error: " << m_reply->error();
        cDebug() << Logger::SubEntry << "Request for url: " << m_reply->url().toString()
                 << " failed with: " << m_reply->errorString();
        m_config->setStatus( Config::Status::FailedNetworkError );
        return;
    }

    QByteArray yamlData = m_reply->readAll();
    try
    {
        auto groups = ::YAML::Load( yamlData.constData() );

        if ( groups.IsSequence() )
        {
            m_config->loadGroupList( Calamares::YAML::sequenceToVariant( groups ) );
            next.done( m_config->statusCode() == Config::Status::Ok );
        }
        else if ( groups.IsMap() )
        {
            auto map = Calamares::YAML::mapToVariant( groups );
            m_config->loadGroupList( map.value( "groups" ).toList() );
            next.done( m_config->statusCode() == Config::Status::Ok );
        }
        else
        {
            cWarning() << "Options groups data does not form a sequence.";
        }
    }
    catch ( ::YAML::Exception& e )
    {
        Calamares::YAML::explainException( e, yamlData, "options groups data" );
        m_config->setStatus( Config::Status::FailedBadData );
    }
}
