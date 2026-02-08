/**
 * @file test_meeting_flow.cpp
 * @brief 会议流程集成测试
 *
 * 测试完整的业务流程，验证多个组件协作的正确性：
 * - 登录 → 创建/加入会议 → 媒体控制 → 离开会议
 * - 反复进出会议的状态一致性
 * - 多模块信号传播链路
 *
 * 注意：这些测试在离线环境运行，不连接真实服务器
 *       重点验证的是状态管理的正确性，而非网络交互
 */

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include "chatmodel.h"
#include "meetingcontroller.h"
#include "livekitmanager.h"
#include "participantmodel.h"

// ==================== 测试夹具 ====================

class MeetingFlowTest : public ::testing::Test
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
        controller = new MeetingController();
        participantModel = new ParticipantModel();
        chatModel = new ChatModel();
    }

    void TearDown() override
    {
        delete chatModel;
        delete participantModel;
        delete controller;
    }

    MeetingController *controller = nullptr;
    ParticipantModel *participantModel = nullptr;
    ChatModel *chatModel = nullptr;
    QCoreApplication *app = nullptr;

    // 辅助方法：模拟登录成功后的状态设置
    void simulateLoginSuccess(const QString &username)
    {
        controller->setUserName(username);
    }

    // 辅助方法：模拟加入会议成功（直接设置状态，跳过网络交互）
    void simulateJoinedMeeting(const QString &roomId, const QString &title)
    {
        controller->setMeetingId(roomId);
        controller->setMeetingTitle(title);
        controller->setInMeeting(true);
    }
};

// ==================== 登录流程测试 ====================

TEST_F(MeetingFlowTest, LoginValidation_EmptyFields)
{
    QSignalSpy failSpy(controller, &MeetingController::loginFailed);

    // 空用户名
    controller->login("", "password");
    EXPECT_EQ(failSpy.count(), 1);

    // 空密码
    controller->login("user", "");
    EXPECT_EQ(failSpy.count(), 2);

    // 全空
    controller->login("", "");
    EXPECT_EQ(failSpy.count(), 3);
}

TEST_F(MeetingFlowTest, LoginThenLogout)
{
    simulateLoginSuccess("张三");
    EXPECT_EQ(controller->userName(), "张三");

    controller->logout();
    EXPECT_TRUE(controller->userName().isEmpty());
}

TEST_F(MeetingFlowTest, LoginThenLogoutDuringMeeting)
{
    simulateLoginSuccess("张三");
    simulateJoinedMeeting("123456", "周会");

    EXPECT_TRUE(controller->isInMeeting());

    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);
    controller->logout();

    // 应先离开会议再登出
    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_EQ(leftSpy.count(), 1);
    EXPECT_TRUE(controller->userName().isEmpty());
}

// ==================== 创建会议流程测试 ====================

TEST_F(MeetingFlowTest, CreateMeeting_GeneratesId)
{
    simulateLoginSuccess("张三");

    controller->createMeeting("技术评审会");

    EXPECT_FALSE(controller->meetingId().isEmpty());
    EXPECT_EQ(controller->meetingTitle(), "技术评审会");
}

TEST_F(MeetingFlowTest, CreateMeeting_DefaultTitle)
{
    simulateLoginSuccess("张三");

    controller->createMeeting("");

    EXPECT_EQ(controller->meetingTitle(), "张三的会议");
}

TEST_F(MeetingFlowTest, CreateMeeting_IdIs9Digits)
{
    simulateLoginSuccess("张三");

    controller->createMeeting("测试");

    QString id = controller->meetingId();
    EXPECT_EQ(id.length(), 9);
    // 验证全是数字
    bool ok;
    id.toLongLong(&ok);
    EXPECT_TRUE(ok);
}

// ==================== 加入会议流程测试 ====================

TEST_F(MeetingFlowTest, JoinMeeting_EmptyId)
{
    simulateLoginSuccess("张三");

    QSignalSpy errorSpy(controller, &MeetingController::errorOccurred);
    controller->joinMeeting("");

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("请输入会议号"));
}

TEST_F(MeetingFlowTest, JoinMeeting_SetsTitleFromId)
{
    simulateLoginSuccess("张三");

    controller->joinMeeting("987654321");

    EXPECT_EQ(controller->meetingId(), "987654321");
    EXPECT_EQ(controller->meetingTitle(), "会议 987654321");
}

// ==================== 离开会议流程测试 ====================

TEST_F(MeetingFlowTest, LeaveMeeting_FullStateReset)
{
    simulateLoginSuccess("张三");
    simulateJoinedMeeting("123456", "周会");

    // 开启一些媒体
    controller->setMicOn(true);
    controller->setCameraOn(true);
    controller->setScreenSharing(true);
    controller->setRecording(true);
    controller->setHandRaised(true);

    // 离开会议
    controller->leaveMeeting();

    // 所有会议相关状态应重置
    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_FALSE(controller->isMicOn());
    EXPECT_FALSE(controller->isCameraOn());
    EXPECT_FALSE(controller->isScreenSharing());
    EXPECT_FALSE(controller->isRecording());
    EXPECT_FALSE(controller->isHandRaised());
    EXPECT_TRUE(controller->meetingId().isEmpty());
    EXPECT_TRUE(controller->meetingTitle().isEmpty());
    EXPECT_EQ(controller->participantCount(), 0);

    // 用户名保留（未登出）
    EXPECT_EQ(controller->userName(), "张三");
}

TEST_F(MeetingFlowTest, LeaveMeeting_EmitSignals)
{
    simulateLoginSuccess("张三");
    simulateJoinedMeeting("123456", "周会");

    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);
    QSignalSpy inMeetingSpy(controller, &MeetingController::inMeetingChanged);
    QSignalSpy msgSpy(controller, &MeetingController::showMessage);

    controller->leaveMeeting();

    EXPECT_EQ(leftSpy.count(), 1);
    EXPECT_GE(inMeetingSpy.count(), 1);
    EXPECT_GE(msgSpy.count(), 1);
}

// ==================== 反复进出会议测试 ====================

TEST_F(MeetingFlowTest, RepeatedJoinLeave_StateConsistency)
{
    simulateLoginSuccess("张三");

    for (int i = 0; i < 5; ++i)
    {
        // 加入会议
        QString roomId = QString::number(100000 + i);
        controller->joinMeeting(roomId);

        EXPECT_EQ(controller->meetingId(), roomId)
            << "第 " << (i + 1) << " 次加入: meetingId 不正确";

        // 模拟连接成功
        simulateJoinedMeeting(roomId, "会议 " + roomId);
        EXPECT_TRUE(controller->isInMeeting())
            << "第 " << (i + 1) << " 次加入: 应在会议中";

        // 开启媒体
        controller->setMicOn(true);
        controller->setCameraOn(true);

        // 离开会议
        controller->leaveMeeting();

        // 验证状态完全重置
        EXPECT_FALSE(controller->isInMeeting())
            << "第 " << (i + 1) << " 次离开: 应已离开会议";
        EXPECT_FALSE(controller->isMicOn())
            << "第 " << (i + 1) << " 次离开: 麦克风应关闭";
        EXPECT_FALSE(controller->isCameraOn())
            << "第 " << (i + 1) << " 次离开: 摄像头应关闭";
        EXPECT_TRUE(controller->meetingId().isEmpty())
            << "第 " << (i + 1) << " 次离开: meetingId 应清空";
    }

    // 用户名始终保持
    EXPECT_EQ(controller->userName(), "张三");
}

TEST_F(MeetingFlowTest, RepeatedJoinLeave_ParticipantModel)
{
    for (int i = 0; i < 3; ++i)
    {
        // 添加参会者
        participantModel->addParticipant("self", "我", true, true);
        participantModel->addParticipant("user1", "张三");
        participantModel->addParticipant("user2", "李四");

        EXPECT_EQ(participantModel->count(), 3)
            << "第 " << (i + 1) << " 次: 应有 3 个参会者";

        // 清空（模拟离开会议）
        participantModel->clear();

        EXPECT_EQ(participantModel->count(), 0)
            << "第 " << (i + 1) << " 次: 清空后应为 0";
    }
}

// ==================== 媒体控制流程测试 ====================

TEST_F(MeetingFlowTest, MediaToggleSequence)
{
    simulateLoginSuccess("张三");
    simulateJoinedMeeting("123456", "周会");

    // 测试快速连续切换
    for (int i = 0; i < 10; ++i)
    {
        controller->toggleMic();
    }
    // 偶数次切换后应回到初始状态
    EXPECT_FALSE(controller->isMicOn());

    for (int i = 0; i < 11; ++i)
    {
        controller->toggleCamera();
    }
    // 奇数次切换后应与初始状态相反
    EXPECT_TRUE(controller->isCameraOn());
}

TEST_F(MeetingFlowTest, EndMeeting_ResetAllState)
{
    simulateLoginSuccess("张三");
    simulateJoinedMeeting("123456", "周会");

    QSignalSpy endSpy(controller, &MeetingController::meetingEnded);
    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);

    controller->endMeeting();

    EXPECT_EQ(endSpy.count(), 1);
    EXPECT_EQ(leftSpy.count(), 1);
    EXPECT_FALSE(controller->isInMeeting());
}

// ==================== 参会者与聊天联动测试 ====================

TEST_F(MeetingFlowTest, ParticipantJoinLeave_WithChat)
{
    // 模拟参会者加入
    participantModel->addParticipant("self", "我", true, true);
    chatModel->addSystemMessage("我 加入了会议");

    participantModel->addParticipant("user1", "张三");
    chatModel->addSystemMessage("张三 加入了会议");

    EXPECT_EQ(participantModel->count(), 2);
    EXPECT_EQ(chatModel->count(), 2);

    // 张三发送消息
    chatModel->addMessage("user1", "张三", "大家好!", false);
    EXPECT_EQ(chatModel->unreadCount(), 1);

    // 张三离开
    participantModel->removeParticipant("user1");
    chatModel->addSystemMessage("张三 离开了会议");

    EXPECT_EQ(participantModel->count(), 1);
    EXPECT_EQ(chatModel->count(), 4); // 2 系统 + 1 聊天 + 1 离开通知
}

// ==================== 凭据持久化流程测试 ====================

TEST_F(MeetingFlowTest, CredentialsPersistence)
{
    // 保存凭据
    controller->saveCredentials("testuser", "testpass");

    EXPECT_TRUE(controller->hasRememberedPassword());
    EXPECT_EQ(controller->getSavedUsername(), "testuser");
    EXPECT_EQ(controller->getSavedPassword(), "testpass");

    // 清除凭据
    controller->clearSavedCredentials();

    EXPECT_FALSE(controller->hasRememberedPassword());
    EXPECT_TRUE(controller->getSavedUsername().isEmpty());
    EXPECT_TRUE(controller->getSavedPassword().isEmpty());
}

// ==================== 异常场景测试 ====================

TEST_F(MeetingFlowTest, LeaveMeetingWhenNotInMeeting)
{
    simulateLoginSuccess("张三");

    // 不在会议中时离开不应崩溃
    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);
    controller->leaveMeeting();

    EXPECT_EQ(leftSpy.count(), 1);
    EXPECT_FALSE(controller->isInMeeting());
}

TEST_F(MeetingFlowTest, EndMeetingWhenNotInMeeting)
{
    simulateLoginSuccess("张三");

    QSignalSpy endSpy(controller, &MeetingController::meetingEnded);
    controller->endMeeting();

    EXPECT_EQ(endSpy.count(), 1);
}

TEST_F(MeetingFlowTest, MediaToggleBeforeJoinMeeting)
{
    simulateLoginSuccess("张三");

    // 未入会时切换媒体不应崩溃
    controller->toggleMic();
    controller->toggleCamera();

    EXPECT_TRUE(controller->isMicOn());
    EXPECT_TRUE(controller->isCameraOn());
}

// ==================== 连接状态流程测试 ====================

TEST_F(MeetingFlowTest, ConnectionState_InitiallyDisconnected)
{
    EXPECT_FALSE(controller->isConnecting());
    EXPECT_FALSE(controller->isConnected());
}

TEST_F(MeetingFlowTest, LiveKitManager_ExposedToQML)
{
    // 验证 LiveKitManager 通过 Q_PROPERTY 暴露
    LiveKitManager *mgr = controller->liveKitManager();
    EXPECT_NE(mgr, nullptr);

    // 验证 MediaCapture 通过 LiveKitManager 可访问
    EXPECT_NE(mgr->mediaCapture(), nullptr);
    EXPECT_NE(mgr->screenCapture(), nullptr);
}

// ==================== 完整流程端到端测试 ====================

TEST_F(MeetingFlowTest, FullFlow_LoginCreateMeetingLeave)
{
    // 1. 登录
    simulateLoginSuccess("张三");
    EXPECT_EQ(controller->userName(), "张三");

    // 2. 创建会议
    controller->createMeeting("技术评审");
    EXPECT_FALSE(controller->meetingId().isEmpty());
    EXPECT_EQ(controller->meetingTitle(), "技术评审");
    QString meetingId = controller->meetingId();

    // 3. 模拟连接成功
    simulateJoinedMeeting(meetingId, "技术评审");
    EXPECT_TRUE(controller->isInMeeting());

    // 4. 添加参会者
    participantModel->addParticipant("self", "张三", true, true);
    participantModel->addParticipant("user1", "李四");

    // 5. 开启媒体
    controller->setMicOn(true);
    controller->setCameraOn(true);
    EXPECT_TRUE(controller->isMicOn());
    EXPECT_TRUE(controller->isCameraOn());

    // 6. 发送聊天消息
    chatModel->sendMessage("会议开始了", "张三");
    EXPECT_EQ(chatModel->count(), 1);

    // 7. 收到消息
    chatModel->addMessage("user1", "李四", "收到", false);
    EXPECT_EQ(chatModel->unreadCount(), 1);

    // 8. 离开会议
    controller->leaveMeeting();
    participantModel->clear();
    chatModel->clear();

    // 9. 验证完全重置
    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_FALSE(controller->isMicOn());
    EXPECT_FALSE(controller->isCameraOn());
    EXPECT_EQ(participantModel->count(), 0);
    EXPECT_EQ(chatModel->count(), 0);

    // 10. 用户仍然登录
    EXPECT_EQ(controller->userName(), "张三");

    // 11. 登出
    controller->logout();
    EXPECT_TRUE(controller->userName().isEmpty());
}

TEST_F(MeetingFlowTest, FullFlow_JoinExistingMeeting)
{
    // 1. 登录
    simulateLoginSuccess("李四");

    // 2. 加入已有会议
    controller->joinMeeting("123456789");
    EXPECT_EQ(controller->meetingId(), "123456789");

    // 3. 模拟连接成功
    simulateJoinedMeeting("123456789", "会议 123456789");

    // 4. 举手
    controller->toggleHandRaise();
    EXPECT_TRUE(controller->isHandRaised());

    // 5. 放手
    controller->toggleHandRaise();
    EXPECT_FALSE(controller->isHandRaised());

    // 6. 离开
    controller->leaveMeeting();
    EXPECT_FALSE(controller->isInMeeting());
}
