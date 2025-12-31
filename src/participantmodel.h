#ifndef PARTICIPANTMODEL_H
#define PARTICIPANTMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QString>

struct Participant
{
    QString id;
    QString name;
    QString avatarUrl;
    bool isMicOn;
    bool isCameraOn;
    bool isScreenSharing;
    bool isHost;
    bool isHandRaised;
    bool isLocal; // 是否为本地用户
};

class ParticipantModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum ParticipantRoles
    {
        IdRole = Qt::UserRole + 1,
        NameRole,
        AvatarUrlRole,
        IsMicOnRole,
        IsCameraOnRole,
        IsScreenSharingRole,
        IsHostRole,
        IsHandRaisedRole,
        IsLocalRole // 新增：本地用户角色
    };

    explicit ParticipantModel(QObject *parent = nullptr);

    // QAbstractListModel接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;

public slots:
    void addParticipant(const QString &id, const QString &name, bool isHost = false, bool isLocal = false);
    void removeParticipant(const QString &id);
    void updateParticipant(const QString &id, bool isMicOn, bool isCameraOn);
    void setParticipantHandRaised(const QString &id, bool raised);
    void setParticipantScreenSharing(const QString &id, bool sharing);
    void clear();

    // 添加模拟参会者
    void addDemoParticipants();

signals:
    void countChanged();

private:
    int findParticipantIndex(const QString &id) const;

private:
    QList<Participant> m_participants;
};

#endif // PARTICIPANTMODEL_H
