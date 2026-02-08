/**
 * @file test_participant_model.cpp
 * @brief ParticipantModel 单元测试
 *
 * 测试内容：
 * - 参会者增删查
 * - 数据模型角色映射
 * - 状态更新与信号发射
 * - 去重逻辑
 * - 清空操作
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>

#include "participantmodel.h"

// ==================== 测试夹具 ====================

class ParticipantModelTest : public ::testing::Test
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
        model = new ParticipantModel();
    }

    void TearDown() override
    {
        delete model;
        model = nullptr;
    }

    ParticipantModel *model = nullptr;
    QCoreApplication *app = nullptr;
};

// ==================== 初始化测试 ====================

TEST_F(ParticipantModelTest, InitiallyEmpty)
{
    EXPECT_EQ(model->count(), 0);
    EXPECT_EQ(model->rowCount(), 0);
}

// ==================== 添加参会者测试 ====================

TEST_F(ParticipantModelTest, AddParticipant)
{
    QSignalSpy countSpy(model, &ParticipantModel::countChanged);

    model->addParticipant("user1", "张三");

    EXPECT_EQ(model->count(), 1);
    EXPECT_EQ(countSpy.count(), 1);
}

TEST_F(ParticipantModelTest, AddMultipleParticipants)
{
    model->addParticipant("user1", "张三");
    model->addParticipant("user2", "李四");
    model->addParticipant("user3", "王五");

    EXPECT_EQ(model->count(), 3);
}

TEST_F(ParticipantModelTest, AddDuplicateParticipantIgnored)
{
    model->addParticipant("user1", "张三");
    model->addParticipant("user1", "张三2"); // 重复 ID

    EXPECT_EQ(model->count(), 1); // 不应增加
}

TEST_F(ParticipantModelTest, AddLocalParticipant)
{
    model->addParticipant("self", "我", true, true);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ParticipantModel::IsLocalRole).toBool());
    EXPECT_TRUE(model->data(index, ParticipantModel::IsHostRole).toBool());
    EXPECT_EQ(model->data(index, ParticipantModel::NameRole).toString(), "我");
}

TEST_F(ParticipantModelTest, AddHostParticipant)
{
    model->addParticipant("host1", "主持人", true, false);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ParticipantModel::IsHostRole).toBool());
    EXPECT_FALSE(model->data(index, ParticipantModel::IsLocalRole).toBool());
}

// ==================== 删除参会者测试 ====================

TEST_F(ParticipantModelTest, RemoveParticipant)
{
    model->addParticipant("user1", "张三");
    model->addParticipant("user2", "李四");
    QSignalSpy countSpy(model, &ParticipantModel::countChanged);

    model->removeParticipant("user1");

    EXPECT_EQ(model->count(), 1);
    EXPECT_EQ(countSpy.count(), 1);

    // 验证剩下的是 user2
    QModelIndex index = model->index(0);
    EXPECT_EQ(model->data(index, ParticipantModel::IdRole).toString(), "user2");
}

TEST_F(ParticipantModelTest, RemoveNonexistentParticipant)
{
    model->addParticipant("user1", "张三");
    QSignalSpy countSpy(model, &ParticipantModel::countChanged);

    model->removeParticipant("nonexistent");

    EXPECT_EQ(model->count(), 1);
    EXPECT_EQ(countSpy.count(), 0); // 不应发射信号
}

// ==================== 数据读取测试 ====================

TEST_F(ParticipantModelTest, DataRoles)
{
    model->addParticipant("user1", "张三", false, false);

    QModelIndex index = model->index(0);
    EXPECT_EQ(model->data(index, ParticipantModel::IdRole).toString(), "user1");
    EXPECT_EQ(model->data(index, ParticipantModel::NameRole).toString(), "张三");
    EXPECT_FALSE(model->data(index, ParticipantModel::IsHostRole).toBool());
    EXPECT_FALSE(model->data(index, ParticipantModel::IsLocalRole).toBool());
    EXPECT_FALSE(model->data(index, ParticipantModel::IsScreenSharingRole).toBool());
    EXPECT_FALSE(model->data(index, ParticipantModel::IsHandRaisedRole).toBool());
}

TEST_F(ParticipantModelTest, LocalParticipantMicCameraDefaults)
{
    model->addParticipant("self", "我", true, true);

    QModelIndex index = model->index(0);
    // 本地用户默认 mic 和 camera 关闭
    EXPECT_FALSE(model->data(index, ParticipantModel::IsMicOnRole).toBool());
    EXPECT_FALSE(model->data(index, ParticipantModel::IsCameraOnRole).toBool());
}

TEST_F(ParticipantModelTest, InvalidIndex)
{
    model->addParticipant("user1", "张三");

    QModelIndex invalidIndex = model->index(999);
    EXPECT_FALSE(model->data(invalidIndex, ParticipantModel::IdRole).isValid());
}

TEST_F(ParticipantModelTest, RoleNames)
{
    auto roles = model->roleNames();
    EXPECT_TRUE(roles.contains(ParticipantModel::IdRole));
    EXPECT_TRUE(roles.contains(ParticipantModel::NameRole));
    EXPECT_TRUE(roles.contains(ParticipantModel::IsMicOnRole));
    EXPECT_TRUE(roles.contains(ParticipantModel::IsCameraOnRole));
    EXPECT_TRUE(roles.contains(ParticipantModel::IsLocalRole));
    EXPECT_TRUE(roles.contains(ParticipantModel::VideoSinkRole));

    EXPECT_EQ(roles[ParticipantModel::IdRole], "participantId");
    EXPECT_EQ(roles[ParticipantModel::NameRole], "name");
}

// ==================== 状态更新测试 ====================

TEST_F(ParticipantModelTest, UpdateParticipant)
{
    model->addParticipant("user1", "张三", false, false);

    model->updateParticipant("user1", true, true);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ParticipantModel::IsMicOnRole).toBool());
    EXPECT_TRUE(model->data(index, ParticipantModel::IsCameraOnRole).toBool());
}

TEST_F(ParticipantModelTest, UpdateParticipantCamera)
{
    model->addParticipant("user1", "张三", false, true);

    model->updateParticipantCamera("user1", true);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ParticipantModel::IsCameraOnRole).toBool());
}

TEST_F(ParticipantModelTest, SetHandRaised)
{
    model->addParticipant("user1", "张三");

    model->setParticipantHandRaised("user1", true);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ParticipantModel::IsHandRaisedRole).toBool());

    model->setParticipantHandRaised("user1", false);
    EXPECT_FALSE(model->data(index, ParticipantModel::IsHandRaisedRole).toBool());
}

TEST_F(ParticipantModelTest, SetScreenSharing)
{
    model->addParticipant("user1", "张三");

    model->setParticipantScreenSharing("user1", true);

    QModelIndex index = model->index(0);
    EXPECT_TRUE(model->data(index, ParticipantModel::IsScreenSharingRole).toBool());
}

TEST_F(ParticipantModelTest, SetVideoSink)
{
    model->addParticipant("user1", "张三");

    // 使用 nullptr 测试（实际应用中会传 QVideoSink*）
    model->setParticipantVideoSink("user1", nullptr);

    QModelIndex index = model->index(0);
    EXPECT_EQ(model->data(index, ParticipantModel::VideoSinkRole).value<QVideoSink *>(), nullptr);
}

// ==================== 清空测试 ====================

TEST_F(ParticipantModelTest, Clear)
{
    model->addParticipant("user1", "张三");
    model->addParticipant("user2", "李四");

    QSignalSpy countSpy(model, &ParticipantModel::countChanged);

    model->clear();

    EXPECT_EQ(model->count(), 0);
    EXPECT_EQ(countSpy.count(), 1);
}

TEST_F(ParticipantModelTest, ClearEmptyModelNoSignal)
{
    QSignalSpy countSpy(model, &ParticipantModel::countChanged);

    model->clear();

    EXPECT_EQ(countSpy.count(), 0); // 空模型不应发射信号
}

// ==================== 演示数据测试 ====================

TEST_F(ParticipantModelTest, AddDemoParticipants)
{
    model->addDemoParticipants();

    // 至少有主持人 "我" + 2 个参会者
    EXPECT_GE(model->count(), 3);

    // 第一个应该是本地主持人
    QModelIndex firstIndex = model->index(0);
    EXPECT_TRUE(model->data(firstIndex, ParticipantModel::IsLocalRole).toBool());
    EXPECT_TRUE(model->data(firstIndex, ParticipantModel::IsHostRole).toBool());
}
