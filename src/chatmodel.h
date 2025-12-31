#ifndef CHATMODEL_H
#define CHATMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QDateTime>

struct ChatMessage
{
    QString id;
    QString senderId;
    QString senderName;
    QString content;
    QDateTime timestamp;
    bool isSystem;
    bool isSelf;
};

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)

public:
    enum ChatRoles
    {
        IdRole = Qt::UserRole + 1,
        SenderIdRole,
        SenderNameRole,
        ContentRole,
        TimestampRole,
        TimeStringRole,
        IsSystemRole,
        IsSelfRole
    };

    explicit ChatModel(QObject *parent = nullptr);

    // QAbstractListModel接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int unreadCount() const;

public slots:
    void sendMessage(const QString &content, const QString &senderName = "我");
    void addSystemMessage(const QString &content);
    void addMessage(const QString &senderId, const QString &senderName, const QString &content, bool isSelf = false);
    void clear();
    void markAllAsRead();

    // 模拟收到消息
    void simulateIncomingMessage();

signals:
    void countChanged();
    void unreadCountChanged();
    void newMessageReceived();

private:
    QString generateMessageId() const;

private:
    QList<ChatMessage> m_messages;
    int m_unreadCount;
    int m_messageIdCounter;
};

#endif // CHATMODEL_H
