#include "participantmodel.h"
#include <QRandomGenerator>
#include <QDebug>

ParticipantModel::ParticipantModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ParticipantModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_participants.count();
}

QVariant ParticipantModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_participants.count())
        return QVariant();

    const Participant &participant = m_participants.at(index.row());

    switch (role)
    {
    case IdRole:
        return participant.id;
    case NameRole:
        return participant.name;
    case AvatarUrlRole:
        return participant.avatarUrl;
    case IsMicOnRole:
        return participant.isMicOn;
    case IsCameraOnRole:
        return participant.isCameraOn;
    case IsScreenSharingRole:
        return participant.isScreenSharing;
    case IsHostRole:
        return participant.isHost;
    case IsHandRaisedRole:
        return participant.isHandRaised;
    case IsLocalRole:
        return participant.isLocal;
    case VideoSinkRole:
        return QVariant::fromValue(participant.videoSink);
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ParticipantModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "participantId";
    roles[NameRole] = "name";
    roles[AvatarUrlRole] = "avatarUrl";
    roles[IsMicOnRole] = "isMicOn";
    roles[IsCameraOnRole] = "isCameraOn";
    roles[IsScreenSharingRole] = "isScreenSharing";
    roles[IsHostRole] = "isHost";
    roles[IsHandRaisedRole] = "isHandRaised";
    roles[IsLocalRole] = "isLocal";
    roles[VideoSinkRole] = "videoSink";
    return roles;
}

int ParticipantModel::count() const
{
    return m_participants.count();
}

void ParticipantModel::addParticipant(const QString &id, const QString &name, bool isHost, bool isLocal)
{
    // 检查是否已存在
    if (findParticipantIndex(id) >= 0)
        return;

    beginInsertRows(QModelIndex(), m_participants.count(), m_participants.count());

    Participant participant;
    participant.id = id;
    participant.name = name;
    participant.avatarUrl = "";
    participant.isMicOn = isLocal ? false : QRandomGenerator::global()->bounded(2) == 1;
    participant.isCameraOn = isLocal ? false : QRandomGenerator::global()->bounded(2) == 1;
    participant.isScreenSharing = false;
    participant.isHost = isHost;
    participant.isHandRaised = false;
    participant.isLocal = isLocal;
    participant.videoSink = nullptr; // 远程视频 Sink 稍后通过 setParticipantVideoSink 设置

    m_participants.append(participant);

    endInsertRows();
    emit countChanged();
}

void ParticipantModel::removeParticipant(const QString &id)
{
    int index = findParticipantIndex(id);
    if (index < 0)
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_participants.removeAt(index);
    endRemoveRows();
    emit countChanged();
}

void ParticipantModel::updateParticipant(const QString &id, bool isMicOn, bool isCameraOn)
{
    int index = findParticipantIndex(id);
    if (index < 0)
        return;

    m_participants[index].isMicOn = isMicOn;
    m_participants[index].isCameraOn = isCameraOn;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsMicOnRole, IsCameraOnRole});
}

void ParticipantModel::updateParticipantCamera(const QString &id, bool isCameraOn)
{
    int index = findParticipantIndex(id);
    if (index < 0)
    {
        qDebug() << "[ParticipantModel] updateParticipantCamera: 未找到参会者" << id;
        return;
    }

    qDebug() << "[ParticipantModel] 更新参会者摄像头状态:" << id << "->" << isCameraOn;
    m_participants[index].isCameraOn = isCameraOn;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsCameraOnRole});
}

void ParticipantModel::setParticipantHandRaised(const QString &id, bool raised)
{
    int index = findParticipantIndex(id);
    if (index < 0)
        return;

    m_participants[index].isHandRaised = raised;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsHandRaisedRole});
}

void ParticipantModel::setParticipantScreenSharing(const QString &id, bool sharing)
{
    int index = findParticipantIndex(id);
    if (index < 0)
        return;

    m_participants[index].isScreenSharing = sharing;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {IsScreenSharingRole});
}

void ParticipantModel::setParticipantVideoSink(const QString &id, QVideoSink *sink)
{
    int index = findParticipantIndex(id);
    if (index < 0)
    {
        qDebug() << "[ParticipantModel] setParticipantVideoSink: 参会者不存在:" << id;
        return;
    }

    qDebug() << "[ParticipantModel] setParticipantVideoSink:" << id << "sink=" << sink;
    m_participants[index].videoSink = sink;

    QModelIndex modelIndex = createIndex(index, 0);
    emit dataChanged(modelIndex, modelIndex, {VideoSinkRole});
}

void ParticipantModel::clear()
{
    if (m_participants.isEmpty())
        return;

    beginResetModel();
    m_participants.clear();
    endResetModel();
    emit countChanged();
}

void ParticipantModel::addDemoParticipants()
{
    QStringList names = {"张三", "李四", "王五", "赵六", "钱七", "孙八", "周九"};

    // 首先添加主持人（自己）- 设置 isLocal=true
    addParticipant("self", "我", true, true);

    // 随机添加几个参会者
    int count = QRandomGenerator::global()->bounded(2, 6);
    for (int i = 0; i < count && i < names.count(); ++i)
    {
        addParticipant(QString::number(i + 1), names[i], false, false);
    }
}

int ParticipantModel::findParticipantIndex(const QString &id) const
{
    for (int i = 0; i < m_participants.count(); ++i)
    {
        if (m_participants[i].id == id)
            return i;
    }
    return -1;
}
