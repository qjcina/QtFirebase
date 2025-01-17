#include "qtfirebasemessaging.h"

#include <QGuiApplication>
#include <QQmlParserStatus>

#include <stdint.h>
#include "firebase/app.h"
#include "firebase/internal/common.h"
#include "firebase/messaging.h"
#include "firebase/util.h"

namespace messaging = ::firebase::messaging;

QtFirebaseMessaging *QtFirebaseMessaging::self = nullptr;

QtFirebaseMessaging::QtFirebaseMessaging(QObject* parent)
    : QObject(parent)
    , g_listener(new MessageListener())
{
    __QTFIREBASE_ID = QString().sprintf("%8p", static_cast<void*> (this));

    if(!self) {
        self = this;
        qDebug() << self << "::QtFirebaseMessaging" << "singleton";
    }

    _ready = false;
    _initializing = false;

    if(qFirebase->ready()) {
        //Call init outside of constructor, otherwise signal readyChanged not emited
        QTimer::singleShot(100, this, &QtFirebaseMessaging::init);
    } else {
        connect(qFirebase,&QtFirebase::readyChanged, this, &QtFirebaseMessaging::init);
        qFirebase->requestInit();
    }

    connect(qFirebase,&QtFirebase::futureEvent, this, &QtFirebaseMessaging::onFutureEvent);
}

QtFirebaseMessaging::~QtFirebaseMessaging()
{
    if(_ready) {
        messaging::Terminate();
    }
}

void QtFirebaseMessaging::classBegin()
{
}

void QtFirebaseMessaging::componentComplete()
{
    // Connect on componentComplete so the signals are emited in the correct order
    connect(g_listener, &MessageListener::onMessageReceived, this, &QtFirebaseMessaging::getMessage);
    connect(g_listener, &MessageListener::onTokenReceived, this, &QtFirebaseMessaging::getToken);
}

bool QtFirebaseMessaging::checkInstance(const char *function)
{
    const bool b = (QtFirebaseMessaging::self != nullptr);
    if (!b)
        qWarning("QtFirebaseMessaging::%s: Please instantiate the QtFirebaseMessaging object first", function);
    return b;
}

void QtFirebaseMessaging::init()
{
    if(!qFirebase->ready()) {
        qDebug() << self << "::init" << "base not ready";
        return;
    }

    if(!_ready && !_initializing) {
        _initializing = true;
        messaging::Initialize(*qFirebase->firebaseApp(), g_listener);
        _initializing = false;
        setReady(true);
    }
}

void QtFirebaseMessaging::onFutureEvent(const QString &eventId, const firebase::FutureBase &future)
{
    if(!eventId.startsWith(__QTFIREBASE_ID))
        return;

    qDebug() << self << "::onFutureEvent" << eventId;;

    if(future.status() != firebase::kFutureStatusComplete)
    {
        qDebug() << this << "::onFutureEvent initializing failed." << "ERROR: Action failed with error code and message: " << future.error() << future.error_message();
        _initializing = false;
        return;
    }
    qDebug() << this << "::onFutureEvent initialized ok";
    _initializing = false;
}

void QtFirebaseMessaging::getMessage()
{
    setData(g_listener->data());
}

void QtFirebaseMessaging::getToken()
{
    setToken(g_listener->token());
}

bool QtFirebaseMessaging::ready()
{
    return _ready;
}

void QtFirebaseMessaging::setReady(bool ready)
{
    if (_ready != ready) {
        _ready = ready;
        emit readyChanged();
    }
}

QVariantMap QtFirebaseMessaging::data()
{
    return _data;
}

void QtFirebaseMessaging::setData(const QVariantMap &data)
{
    if (_data != data) {
        _data = data;
        emit dataChanged();
        emit messageReceived();
    }
}

QString QtFirebaseMessaging::token()
{
    return _token;
}

void QtFirebaseMessaging::setToken(const QString &token)
{
    if (_token != token) {
        _token = token;
        emit tokenChanged();
    }
}

void QtFirebaseMessaging::subscribe(const QString &topic)
{
    // TODO queue these futures so repeated calls don't get lost
    auto result = firebase::messaging::Subscribe(topic.toUtf8());

    result.OnCompletion([this, topic](const firebase::FutureBase &completed_future){
        if(completed_future.error() == firebase::messaging::kErrorNone) {
            emit subscribed(topic);
        } else {
            emit error(completed_future.error(), QString(QString::fromUtf8(completed_future.error_message())));
        }
    });
}

void QtFirebaseMessaging::unsubscribe(const QString &topic)
{
    // TODO queue these futures so repeated calls don't get lost
    auto result = firebase::messaging::Unsubscribe(topic.toUtf8());

    result.OnCompletion([this, topic](const firebase::FutureBase &completed_future){
        if(completed_future.error() == firebase::messaging::kErrorNone) {
            emit unsubscribed(topic);
        } else {
            emit error(completed_future.error(), QString(QString::fromUtf8(completed_future.error_message())));
        }
    });
}

MessageListener::MessageListener(QObject* parent)
    : QObject(parent)
{
}

void MessageListener::OnMessage(const messaging::Message &message)
{
    // When messages are received by the server, they are placed into an
    // internal queue, waiting to be consumed. When ProcessMessages is called,
    // this OnMessage function is called once for each queued message.

    QVariantMap data;

    if (message.notification) {
        if (!message.notification->title.empty()) {
            const QString key = QStringLiteral("nTitle");
            const QString value = QString::fromStdString(message.notification->title.c_str());
            data.insert(key, value);
        }
        if (!message.notification->body.empty()) {
            const QString key = QStringLiteral("nBody");
            const QString value = QString::fromStdString(message.notification->body.c_str());
            data.insert(key, value);
        }
        if (!message.notification->icon.empty()) {
            const QString key = QStringLiteral("nIcon");
            const QString value = QString::fromStdString(message.notification->icon.c_str());
            data.insert(key, value);
        }
        if (!message.notification->tag.empty()) {
            const QString key = QStringLiteral("nTag");
            const QString value = QString::fromStdString(message.notification->tag.c_str());
            data.insert(key, value);
        }
        if (!message.notification->color.empty()) {
            const QString key = QStringLiteral("nColor");
            const QString value = QString::fromStdString(message.notification->color.c_str());
            data.insert(key, value);
        }
        if (!message.notification->sound.empty()) {
            const QString key = QStringLiteral("nSound");
            const QString value = QString::fromStdString(message.notification->sound.c_str());
            data.insert(key, value);
        }
        if (!message.notification->click_action.empty()) {
            const QString key = QStringLiteral("nClickAction");
            const QString value = QString::fromStdString(message.notification->click_action.c_str());
            data.insert(key, value);
        }
    }

    if (message.notification_opened) {
        const QString key = QStringLiteral("launchnotification");
        data.insert(key, true);
    }

    for (const auto& field : message.data)
    {
        const QString key = QString::fromStdString(field.first);
        const QString value = QString::fromStdString(field.second);

        data.insert(key, value);
    }

    setData(data);
}

void MessageListener::OnTokenReceived(const char *token)
{
    setToken(QString::fromUtf8(token));
}

void MessageListener::connectNotify(const QMetaMethod &signal)
{
    if (signal == QMetaMethod::fromSignal(&MessageListener::onMessageReceived)) {
        _messageReceivedConnected = true;

        if(_notifyMessageReceived) {
            emit onMessageReceived();
            _notifyMessageReceived = false;
        }
    }

    if (signal == QMetaMethod::fromSignal(&MessageListener::onTokenReceived)) {
        _tokenReceivedConnected = true;

        if(_notifyTokenReceived) {
            emit onTokenReceived();
            _notifyTokenReceived = false;
        }
    }
}

QVariantMap MessageListener::data()
{
    return _data;
}

void MessageListener::setData(const QVariantMap &data)
{
    if (_data != data) {
        _notifyMessageReceived = true;
        _data = data;

        if(_messageReceivedConnected) {
            emit onMessageReceived();
            _notifyMessageReceived = false;
        }
    }
}

QString MessageListener::token()
{
    return _token;
}

void MessageListener::setToken(const QString &token)
{
    if (_token != token) {
        _notifyTokenReceived = true;
        _token = token;

        if(_tokenReceivedConnected) {
            emit onTokenReceived();
            _notifyTokenReceived = false;
        }
    }
}
