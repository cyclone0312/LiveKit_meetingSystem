/**
 * @file test_chat_model.cpp
 * @brief ChatModel 单元测试
 *
 * 测试内容：
 * - 发送消息
 * - 系统消息
 * - 未读计数
 * - 数据角色映射
 * - 清空操作
 * - 消息 ID 唯一性
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>

#include "chatmodel.h"

// ==================== 测试夹具 ====================

class ChatModelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QCoreApplication::instance())
        {
            static int argc = 1;
            static char *argv[] = {(char *)"test"};
            app = new QCoreApplication(argc, argv);
        }
        model = new ChatModel();
    }

    void TearDown() override
    {
        delete model;
        model = nullptr;
    }

    ChatModel *model = nullptr;
    QCoreApplication *app = nullptr;
};

// ==================== 初始化测试 ====================

TEST_F(ChatModelTest, InitiallyEmpty)
{
    EXPECT_EQ(model->count(), 0);
    EXPECT_EQ(model->unreadCount(), 0);
    EXPECT_EQ(model->rowCount(), 0);
}

// ==================== 发送消息测试 ====================

TEST_F(ChatModelTest, SendMessage)
{
    QSignalSpy countSpy(model, &ChatModel::countChanged);
    QSignalSpy newMsgSpy(model, &ChatModel::newMessageReceived);

    model->sendMessage("Hello, World!", "张三");

    EXPECT_EQ(model->count(), 1);
    EXPECT_EQ(countSpy.count(), 1);
    EXPECT_EQ(newMsgSpy.count(), 1);
}

TEST_F(ChatModelTest, SendEmptyMessageIgnored)
{
    model->sendMessage("", "张三");
    model->sendMessage("   ", "张三"); // 仅空白

    EXPECT_EQ(model->count(), 0);
}

TEST_F(ChatModelTest, SendMessageIsSelf)
{
    model->sendMessage("测试消息", "我");

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ChatModel::IsSelfRole).toBool());
    EXPECT_FALSE(model->data(index, ChatModel::IsSystemRole).toBool());
    EXPECT_EQ(model->data(index, ChatModel::ContentRole).toString(), "测试消息");
    EXPECT_EQ(model->data(index, ChatModel::SenderNameRole).toString(), "我");
}

TEST_F(ChatModelTest, SelfMessageNoUnread)
{
    model->sendMessage("测试消息", "我");

    // 自己发的消息不应增加未读计数
    EXPECT_EQ(model->unreadCount(), 0);
}

// ==================== 接收消息测试 ====================

TEST_F(ChatModelTest, AddMessageFromOthers)
{
    QSignalSpy unreadSpy(model, &ChatModel::unreadCountChanged);

    model->addMessage("user1", "张三", "你好！", false);

    EXPECT_EQ(model->count(), 1);
    EXPECT_EQ(model->unreadCount(), 1);
    EXPECT_EQ(unreadSpy.count(), 1);

    QModelIndex index = model->index(0);
    EXPECT_FALSE(model->data(index, ChatModel::IsSelfRole).toBool());
    EXPECT_EQ(model->data(index, ChatModel::SenderIdRole).toString(), "user1");
}

TEST_F(ChatModelTest, MultipleUnreadMessages)
{
    model->addMessage("user1", "张三", "消息1", false);
    model->addMessage("user2", "李四", "消息2", false);
    model->addMessage("user3", "王五", "消息3", false);

    EXPECT_EQ(model->unreadCount(), 3);
}

// ==================== 系统消息测试 ====================

TEST_F(ChatModelTest, AddSystemMessage)
{
    model->addSystemMessage("张三加入了会议");

    EXPECT_EQ(model->count(), 1);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ChatModel::IsSystemRole).toBool());
    EXPECT_FALSE(model->data(index, ChatModel::IsSelfRole).toBool());
    EXPECT_EQ(model->data(index, ChatModel::SenderNameRole).toString(), "系统消息");
    EXPECT_EQ(model->data(index, ChatModel::ContentRole).toString(), "张三加入了会议");
}

// ==================== 数据角色测试 ====================

TEST_F(ChatModelTest, DataRoles)
{
    model->sendMessage("测试内容", "发送者");

    QModelIndex index = model->index(0);

    // 验证所有角色都有值
    EXPECT_FALSE(model->data(index, ChatModel::IdRole).toString().isEmpty());
    EXPECT_EQ(model->data(index, ChatModel::SenderIdRole).toString(), "self");
    EXPECT_EQ(model->data(index, ChatModel::SenderNameRole).toString(), "发送者");
    EXPECT_EQ(model->data(index, ChatModel::ContentRole).toString(), "测试内容");
    EXPECT_TRUE(model->data(index, ChatModel::TimestampRole).toDateTime().isValid());
    EXPECT_FALSE(model->data(index, ChatModel::TimeStringRole).toString().isEmpty());
}

TEST_F(ChatModelTest, TimeStringFormat)
{
    model->sendMessage("测试", "我");

    QModelIndex index = model->index(0);
    QString timeStr = model->data(index, ChatModel::TimeStringRole).toString();

    // 格式应为 "hh:mm"
    EXPECT_EQ(timeStr.length(), 5);
    EXPECT_EQ(timeStr.at(2), ':');
}

TEST_F(ChatModelTest, RoleNames)
{
    auto roles = model->roleNames();
    EXPECT_TRUE(roles.contains(ChatModel::IdRole));
    EXPECT_TRUE(roles.contains(ChatModel::ContentRole));
    EXPECT_TRUE(roles.contains(ChatModel::SenderNameRole));
    EXPECT_TRUE(roles.contains(ChatModel::IsSystemRole));

    EXPECT_EQ(roles[ChatModel::ContentRole], "content");
    EXPECT_EQ(roles[ChatModel::SenderNameRole], "senderName");
}

TEST_F(ChatModelTest, InvalidIndex)
{
    model->sendMessage("测试", "我");

    QModelIndex invalidIndex = model->index(999);
    EXPECT_FALSE(model->data(invalidIndex, ChatModel::IdRole).isValid());
}

// ==================== 未读标记测试 ====================

TEST_F(ChatModelTest, MarkAllAsRead)
{
    model->addMessage("user1", "张三", "消息1", false);
    model->addMessage("user2", "李四", "消息2", false);
    EXPECT_EQ(model->unreadCount(), 2);

    QSignalSpy unreadSpy(model, &ChatModel::unreadCountChanged);
    model->markAllAsRead();

    EXPECT_EQ(model->unreadCount(), 0);
    EXPECT_EQ(unreadSpy.count(), 1);
}

TEST_F(ChatModelTest, MarkAllAsReadNoSignalWhenZero)
{
    // 未读为 0 时标记不应发射信号
    QSignalSpy unreadSpy(model, &ChatModel::unreadCountChanged);

    model->markAllAsRead();

    EXPECT_EQ(unreadSpy.count(), 0);
}

// ==================== 清空测试 ====================

TEST_F(ChatModelTest, Clear)
{
    model->sendMessage("消息1", "我");
    model->addMessage("user1", "张三", "消息2", false);
    model->addSystemMessage("系统通知");

    QSignalSpy countSpy(model, &ChatModel::countChanged);
    QSignalSpy unreadSpy(model, &ChatModel::unreadCountChanged);

    model->clear();

    EXPECT_EQ(model->count(), 0);
    EXPECT_EQ(model->unreadCount(), 0);
    EXPECT_EQ(countSpy.count(), 1);
    EXPECT_EQ(unreadSpy.count(), 1);
}

TEST_F(ChatModelTest, ClearEmptyModelNoSignal)
{
    QSignalSpy countSpy(model, &ChatModel::countChanged);

    model->clear();

    EXPECT_EQ(countSpy.count(), 0);
}

// ==================== 模拟消息测试 ====================

TEST_F(ChatModelTest, SimulateIncomingMessage)
{
    QSignalSpy newMsgSpy(model, &ChatModel::newMessageReceived);

    model->simulateIncomingMessage();

    EXPECT_EQ(model->count(), 1);
    EXPECT_EQ(model->unreadCount(), 1);
    EXPECT_EQ(newMsgSpy.count(), 1);

    // 验证消息内容不为空
    QModelIndex index = model->index(0);
    EXPECT_FALSE(model->data(index, ChatModel::ContentRole).toString().isEmpty());
    EXPECT_FALSE(model->data(index, ChatModel::SenderNameRole).toString().isEmpty());
}

// ==================== 消息 ID 唯一性测试 ====================

TEST_F(ChatModelTest, MessageIdsAreUnique)
{
    model->sendMessage("消息1", "我");
    model->sendMessage("消息2", "我");
    model->addSystemMessage("系统消息");

    QSet<QString> ids;
    for (int i = 0; i < model->count(); ++i)
    {
        QModelIndex index = model->index(i);
        QString id = model->data(index, ChatModel::IdRole).toString();
        EXPECT_FALSE(ids.contains(id)) << "重复的消息 ID: " << id.toStdString();
        ids.insert(id);
    }
}

// ==================== 消息顺序测试 ====================

TEST_F(ChatModelTest, MessagesInOrder)
{
    model->sendMessage("第一条", "我");
    model->addMessage("user1", "张三", "第二条", false);
    model->addSystemMessage("第三条");

    EXPECT_EQ(model->count(), 3);

    QModelIndex idx0 = model->index(0);
    QModelIndex idx1 = model->index(1);
    QModelIndex idx2 = model->index(2);

    EXPECT_EQ(model->data(idx0, ChatModel::ContentRole).toString(), "第一条");
    EXPECT_EQ(model->data(idx1, ChatModel::ContentRole).toString(), "第二条");
    EXPECT_EQ(model->data(idx2, ChatModel::ContentRole).toString(), "第三条");
}
