#include "chatmodel.h"
#include <QRandomGenerator>
#include <QtGlobal>

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent), m_unreadCount(0), m_messageIdCounter(0)
{
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_messages.count();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.count())
        return QVariant();

    const ChatMessage &message = m_messages.at(index.row());

    switch (role)
    {
    case IdRole:
        return message.id;
    case SenderIdRole:
        return message.senderId;
    case SenderNameRole:
        return message.senderName;
    case ContentRole:
        return message.content;
    case TimestampRole:
        return message.timestamp;
    case TimeStringRole:
        return message.timestamp.toString("hh:mm");
    case IsSystemRole:
        return message.isSystem;
    case IsSelfRole:
        return message.isSelf;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "messageId";
    roles[SenderIdRole] = "senderId";
    roles[SenderNameRole] = "senderName";
    roles[ContentRole] = "content";
    roles[TimestampRole] = "timestamp";
    roles[TimeStringRole] = "timeString";
    roles[IsSystemRole] = "isSystem";
    roles[IsSelfRole] = "isSelf";
    return roles;
}

int ChatModel::count() const
{
    return m_messages.count();
}

int ChatModel::unreadCount() const
{
    return m_unreadCount;
}

bool ChatModel::registerRecentMessageAlias(const QString &externalId,
                                           const QString &senderId,
                                           const QString &senderName,
                                           const QString &content,
                                           bool isSelf,
                                           const QDateTime &timestamp,
                                           int maxAgeMs)
{
    if (externalId.isEmpty())
        return false;

    if (m_externalMessageIds.contains(externalId))
        return true;

    const QString trimmedContent = content.trimmed();
    if (trimmedContent.isEmpty())
        return false;

    const QDateTime referenceTime =
        timestamp.isValid() ? timestamp : QDateTime::currentDateTime();

    for (int i = m_messages.count() - 1; i >= 0; --i)
    {
        const ChatMessage &message = m_messages.at(i);
        if (message.isSystem)
            continue;

        if (message.senderId != senderId || message.senderName != senderName ||
            message.isSelf != isSelf || message.content != trimmedContent)
        {
            continue;
        }

        const qint64 ageMs = qAbs(message.timestamp.msecsTo(referenceTime));
        if (ageMs <= maxAgeMs)
        {
            m_externalMessageIds.insert(externalId);
            return true;
        }
    }

    return false;
}

void ChatModel::sendMessage(const QString &content, const QString &senderName)
{
    if (content.trimmed().isEmpty())
        return;

    addMessage("self", senderName, content, true);
}

void ChatModel::addSystemMessage(const QString &content)
{
    appendMessage(QString(), "system", "系统消息", content, true, false);
}

void ChatModel::addMessage(const QString &senderId, const QString &senderName,
                           const QString &content, bool isSelf)
{
    appendMessage(QString(), senderId, senderName, content, false, isSelf);
}

void ChatModel::addMessageWithId(const QString &externalId,
                                 const QString &senderId,
                                 const QString &senderName,
                                 const QString &content, bool isSelf)
{
    appendMessage(externalId, senderId, senderName, content, false, isSelf);
}

void ChatModel::addMessageWithIdAndTimestamp(const QString &externalId,
                                             const QString &senderId,
                                             const QString &senderName,
                                             const QString &content,
                                             bool isSelf,
                                             const QDateTime &timestamp)
{
    appendMessage(externalId, senderId, senderName, content, false, isSelf,
                  timestamp);
}

bool ChatModel::appendMessage(const QString &externalId, const QString &senderId,
                              const QString &senderName,
                              const QString &content, bool isSystem,
                              bool isSelf, const QDateTime &timestamp)
{
    const QString trimmedContent = content.trimmed();
    if (trimmedContent.isEmpty())
        return false;

    if (!externalId.isEmpty() && m_externalMessageIds.contains(externalId))
        return false;

    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());

    ChatMessage message;
    message.id = externalId.isEmpty() ? generateMessageId() : externalId;
    message.senderId = senderId;
    message.senderName = senderName;
    message.content = trimmedContent;
    message.timestamp = timestamp.isValid() ? timestamp : QDateTime::currentDateTime();
    message.isSystem = isSystem;
    message.isSelf = isSelf;

    m_messages.append(message);
    if (!externalId.isEmpty())
        m_externalMessageIds.insert(externalId);

    endInsertRows();
    emit countChanged();

    if (!isSystem && !isSelf)
    {
        m_unreadCount++;
        emit unreadCountChanged();
    }

    if (!isSystem)
    {
        emit newMessageReceived();
    }

    return true;
}

void ChatModel::clear()
{
    if (m_messages.isEmpty())
        return;

    beginResetModel();
    m_messages.clear();
    m_externalMessageIds.clear();
    m_unreadCount = 0;
    endResetModel();
    emit countChanged();
    emit unreadCountChanged();
}

void ChatModel::markAllAsRead()
{
    if (m_unreadCount > 0)
    {
        m_unreadCount = 0;
        emit unreadCountChanged();
    }
}

void ChatModel::simulateIncomingMessage()
{
    QStringList names = {"张三", "李四", "王五", "赵六"};
    QStringList messages = {
        "大家好！",
        "能听到吗？",
        "收到，可以听到",
        "请问现在可以开始了吗？",
        "好的，明白了",
        "稍等一下",
        "没问题",
        "我这边网络有点卡"};

    QString name = names[QRandomGenerator::global()->bounded(names.count())];
    QString msg = messages[QRandomGenerator::global()->bounded(messages.count())];

    addMessage(QString::number(QRandomGenerator::global()->bounded(100)), name, msg, false);
}

QString ChatModel::generateMessageId() const
{
    // Use const_cast to increment counter in const method
    // (counter is an implementation detail for uniqueness)
    int currentCount = m_messageIdCounter;
    const_cast<ChatModel *>(this)->m_messageIdCounter = currentCount + 1;
    return QString("msg_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(currentCount);
}
