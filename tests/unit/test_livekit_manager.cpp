/**
 * @file test_livekit_manager.cpp
 * @brief LiveKitManager 单元测试
 *
 * 测试内容：
 * - 初始化状态
 * - 属性设置与信号
 * - 空字段校验（joinRoom）
 * - 重复连接保护
 * - 媒体组件存在性
 *
 * 注意：这些是离线测试，不会真正连接 LiveKit 服务器
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>

#include "livekitmanager.h"

// ==================== 测试夹具 ====================

class LiveKitManagerTest : public ::testing::Test
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
        manager = new LiveKitManager();
    }

    void TearDown() override
    {
        delete manager;
        manager = nullptr;
    }

    LiveKitManager *manager = nullptr;
    QCoreApplication *app = nullptr;
};

// ==================== 初始化测试 ====================

TEST_F(LiveKitManagerTest, InitialState)
{
    EXPECT_FALSE(manager->isConnected());
    EXPECT_FALSE(manager->isConnecting());
    EXPECT_TRUE(manager->currentRoom().isEmpty());
    EXPECT_TRUE(manager->currentUser().isEmpty());
    EXPECT_TRUE(manager->errorMessage().isEmpty());
}

TEST_F(LiveKitManagerTest, DefaultServerUrls)
{
    // 默认服务器配置不应为空
    EXPECT_FALSE(manager->serverUrl().isEmpty());
    EXPECT_FALSE(manager->tokenServerUrl().isEmpty());
}

TEST_F(LiveKitManagerTest, MediaCaptureExists)
{
    EXPECT_NE(manager->mediaCapture(), nullptr);
}

TEST_F(LiveKitManagerTest, ScreenCaptureExists)
{
    EXPECT_NE(manager->screenCapture(), nullptr);
}

TEST_F(LiveKitManagerTest, RoomObjectExists)
{
    EXPECT_NE(manager->room(), nullptr);
}

// ==================== 媒体发布状态初始化 ====================

TEST_F(LiveKitManagerTest, InitialPublishState)
{
    EXPECT_FALSE(manager->isCameraPublished());
    EXPECT_FALSE(manager->isMicrophonePublished());
    EXPECT_FALSE(manager->isScreenSharePublished());
}

// ==================== 属性设置测试 ====================

TEST_F(LiveKitManagerTest, SetServerUrl)
{
    QSignalSpy spy(manager, &LiveKitManager::serverUrlChanged);

    manager->setServerUrl("ws://localhost:7880");

    EXPECT_EQ(manager->serverUrl(), "ws://localhost:7880");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(LiveKitManagerTest, SetServerUrlNoSignalOnSameValue)
{
    QString originalUrl = manager->serverUrl();
    QSignalSpy spy(manager, &LiveKitManager::serverUrlChanged);

    manager->setServerUrl(originalUrl);

    EXPECT_EQ(spy.count(), 0);
}

TEST_F(LiveKitManagerTest, SetTokenServerUrl)
{
    QSignalSpy spy(manager, &LiveKitManager::tokenServerUrlChanged);

    manager->setTokenServerUrl("http://localhost:3000");

    EXPECT_EQ(manager->tokenServerUrl(), "http://localhost:3000");
    EXPECT_EQ(spy.count(), 1);
}

// ==================== joinRoom 校验测试 ====================

TEST_F(LiveKitManagerTest, JoinRoomEmptyRoomName)
{
    QSignalSpy errorSpy(manager, &LiveKitManager::errorOccurred);

    manager->joinRoom("", "testuser");

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("不能为空"));
}

TEST_F(LiveKitManagerTest, JoinRoomEmptyUserName)
{
    QSignalSpy errorSpy(manager, &LiveKitManager::errorOccurred);

    manager->joinRoom("room123", "");

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("不能为空"));
}

TEST_F(LiveKitManagerTest, JoinRoomEmptyBoth)
{
    QSignalSpy errorSpy(manager, &LiveKitManager::errorOccurred);

    manager->joinRoom("", "");

    EXPECT_EQ(errorSpy.count(), 1);
}

TEST_F(LiveKitManagerTest, JoinRoomSetsConnecting)
{
    // 提供有效参数，应进入 connecting 状态
    // 注意：实际不会成功连接（网络不可达也没关系，关键是状态变更）
    QSignalSpy stateSpy(manager, &LiveKitManager::connectionStateChanged);

    manager->joinRoom("testroom", "testuser");

    EXPECT_TRUE(manager->isConnecting());
    EXPECT_GE(stateSpy.count(), 1);
}

// ==================== 发布控制校验（未连接时）====================

TEST_F(LiveKitManagerTest, PublishCameraWithoutConnection)
{
    QSignalSpy errorSpy(manager, &LiveKitManager::errorOccurred);

    manager->publishCamera();

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("未连接"));
}

TEST_F(LiveKitManagerTest, PublishMicrophoneWithoutConnection)
{
    QSignalSpy errorSpy(manager, &LiveKitManager::errorOccurred);

    manager->publishMicrophone();

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("未连接"));
}

TEST_F(LiveKitManagerTest, PublishScreenShareWithoutConnection)
{
    QSignalSpy errorSpy(manager, &LiveKitManager::errorOccurred);

    manager->publishScreenShare();

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("未连接"));
}

// ==================== leaveRoom 测试 ====================

TEST_F(LiveKitManagerTest, LeaveRoomWhenNotConnected)
{
    QSignalSpy disconnectSpy(manager, &LiveKitManager::disconnected);

    // 即使未连接也应能安全调用
    manager->leaveRoom();

    // 应发射 disconnected 信号
    EXPECT_EQ(disconnectSpy.count(), 1);

    EXPECT_FALSE(manager->isConnected());
    EXPECT_FALSE(manager->isConnecting());
    EXPECT_TRUE(manager->currentRoom().isEmpty());
}

TEST_F(LiveKitManagerTest, LeaveRoomClearsPublishState)
{
    manager->leaveRoom();

    EXPECT_FALSE(manager->isCameraPublished());
    EXPECT_FALSE(manager->isMicrophonePublished());
    EXPECT_FALSE(manager->isScreenSharePublished());
}

// ==================== setUserPassword 测试 ====================

TEST_F(LiveKitManagerTest, SetUserPassword)
{
    // 不应崩溃
    manager->setUserPassword("testpassword");
    // setUserPassword 没有 getter，只验证不崩溃
}

// ==================== 兼容接口测试 ====================

TEST_F(LiveKitManagerTest, SendChatMessageWhenNotConnected)
{
    // 未连接时发送不应崩溃
    manager->sendChatMessage("测试消息");
    // 只验证不崩溃
}

TEST_F(LiveKitManagerTest, UpdateMicState)
{
    // 未连接时调用不应崩溃
    manager->updateMicState(true);
    manager->updateMicState(false);
}

TEST_F(LiveKitManagerTest, UpdateCameraState)
{
    manager->updateCameraState(true);
    manager->updateCameraState(false);
}
