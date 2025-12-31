#include "chatmodel.h"
#include <QRandomGenerator>

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

void ChatModel::sendMessage(const QString &content, const QString &senderName)
{
    if (content.trimmed().isEmpty())
        return;

    addMessage("self", senderName, content, true);
}

void ChatModel::addSystemMessage(const QString &content)
{
    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());

    ChatMessage message;
    message.id = generateMessageId();
    message.senderId = "system";
    message.senderName = "系统消息";
    message.content = content;
    message.timestamp = QDateTime::currentDateTime();
    message.isSystem = true;
    message.isSelf = false;

    m_messages.append(message);

    endInsertRows();
    emit countChanged();
}

void ChatModel::addMessage(const QString &senderId, const QString &senderName,
                           const QString &content, bool isSelf)
{
    beginInsertRows(QModelIndex(), m_messages.count(), m_messages.count());

    ChatMessage message;
    message.id = generateMessageId();
    message.senderId = senderId;
    message.senderName = senderName;
    message.content = content;
    message.timestamp = QDateTime::currentDateTime();
    message.isSystem = false;
    message.isSelf = isSelf;

    m_messages.append(message);

    endInsertRows();
    emit countChanged();

    if (!isSelf)
    {
        m_unreadCount++;
        emit unreadCountChanged();
    }

    emit newMessageReceived();
}

void ChatModel::clear()
{
    if (m_messages.isEmpty())
        return;

    beginResetModel();
    m_messages.clear();
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
    return QString("msg_%1_%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(m_messageIdCounter);
}
