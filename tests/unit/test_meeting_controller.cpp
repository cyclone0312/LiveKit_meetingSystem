/**
 * @file test_meeting_controller.cpp
 * @brief MeetingController 单元测试
 *
 * 测试内容：
 * - 登录验证逻辑（空字段校验）
 * - 加入/离开会议状态管理
 * - 媒体控制状态切换（麦克风、摄像头）
 * - 会议时长计时器
 * - 凭据保存/读取
 * - 属性变更信号发射
 */

#include <gtest/gtest.h>

#include <QGuiApplication>
#include <QSignalSpy>
#include <QTest>

#include "meetingcontroller.h"
#include "livekitmanager.h"

// ==================== 测试夹具 ====================

class MeetingControllerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // GoogleTest 需要 QCoreApplication 实例（Qt 信号/槽依赖事件循环）
        if (!QGuiApplication::instance())
        {
            static int argc = 1;
            static char *argv[] = {(char *)"test"};
            app = new QGuiApplication(argc, argv);
        }
        controller = new MeetingController();
    }

    void TearDown() override
    {
        delete controller;
        controller = nullptr;
    }

    MeetingController *controller = nullptr;
    QGuiApplication *app = nullptr;
};

// ==================== 初始化状态测试 ====================

TEST_F(MeetingControllerTest, InitialState)
{
    // 验证所有状态属性初始值
    EXPECT_FALSE(controller->isMicOn());
    EXPECT_FALSE(controller->isCameraOn());
    EXPECT_FALSE(controller->isScreenSharing());
    EXPECT_FALSE(controller->isRecording());
    EXPECT_FALSE(controller->isHandRaised());
    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_TRUE(controller->meetingId().isEmpty());
    EXPECT_TRUE(controller->userName().isEmpty());
    EXPECT_TRUE(controller->meetingTitle().isEmpty());
    EXPECT_EQ(controller->participantCount(), 0);
    EXPECT_EQ(controller->meetingDuration(), "00:00");
}

TEST_F(MeetingControllerTest, LiveKitManagerExists)
{
    // 验证 LiveKitManager 已创建
    EXPECT_NE(controller->liveKitManager(), nullptr);
}

// ==================== 登录测试 ====================

TEST_F(MeetingControllerTest, LoginWithEmptyUsername)
{
    QSignalSpy failSpy(controller, &MeetingController::loginFailed);

    controller->login("", "password123");

    EXPECT_EQ(failSpy.count(), 1);
    QList<QVariant> args = failSpy.takeFirst();
    EXPECT_TRUE(args.at(0).toString().contains("不能为空"));
}

TEST_F(MeetingControllerTest, LoginWithEmptyPassword)
{
    QSignalSpy failSpy(controller, &MeetingController::loginFailed);

    controller->login("testuser", "");

    EXPECT_EQ(failSpy.count(), 1);
    QList<QVariant> args = failSpy.takeFirst();
    EXPECT_TRUE(args.at(0).toString().contains("不能为空"));
}

TEST_F(MeetingControllerTest, LoginWithEmptyBoth)
{
    QSignalSpy failSpy(controller, &MeetingController::loginFailed);

    controller->login("", "");

    EXPECT_EQ(failSpy.count(), 1);
}

TEST_F(MeetingControllerTest, LoginWithValidCredentials)
{
    // login() 应该调用 LiveKitManager::loginUser()
    // 这里只验证不会因为空字段被拦截
    QSignalSpy failSpy(controller, &MeetingController::loginFailed);

    controller->login("testuser", "password123");

    // 不应触发空字段错误
    EXPECT_EQ(failSpy.count(), 0);
}

// ==================== 属性设置测试 ====================

TEST_F(MeetingControllerTest, SetUserName)
{
    QSignalSpy spy(controller, &MeetingController::userNameChanged);

    controller->setUserName("张三");

    EXPECT_EQ(controller->userName(), "张三");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(MeetingControllerTest, SetUserNameNoSignalOnSameValue)
{
    controller->setUserName("张三");
    QSignalSpy spy(controller, &MeetingController::userNameChanged);

    controller->setUserName("张三"); // 相同值

    EXPECT_EQ(spy.count(), 0); // 不应重复发射信号
}

TEST_F(MeetingControllerTest, SetMeetingId)
{
    QSignalSpy spy(controller, &MeetingController::meetingIdChanged);

    controller->setMeetingId("123456789");

    EXPECT_EQ(controller->meetingId(), "123456789");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(MeetingControllerTest, SetMeetingTitle)
{
    QSignalSpy spy(controller, &MeetingController::meetingTitleChanged);

    controller->setMeetingTitle("技术评审会");

    EXPECT_EQ(controller->meetingTitle(), "技术评审会");
    EXPECT_EQ(spy.count(), 1);
}

// ==================== 媒体控制测试 ====================

TEST_F(MeetingControllerTest, ToggleMic)
{
    QSignalSpy spy(controller, &MeetingController::micOnChanged);

    EXPECT_FALSE(controller->isMicOn());

    controller->toggleMic();
    EXPECT_TRUE(controller->isMicOn());
    EXPECT_EQ(spy.count(), 1);

    controller->toggleMic();
    EXPECT_FALSE(controller->isMicOn());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(MeetingControllerTest, ToggleCamera)
{
    QSignalSpy spy(controller, &MeetingController::cameraOnChanged);

    EXPECT_FALSE(controller->isCameraOn());

    controller->toggleCamera();
    EXPECT_TRUE(controller->isCameraOn());
    EXPECT_EQ(spy.count(), 1);

    controller->toggleCamera();
    EXPECT_FALSE(controller->isCameraOn());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(MeetingControllerTest, ToggleHandRaise)
{
    QSignalSpy spy(controller, &MeetingController::handRaisedChanged);

    controller->toggleHandRaise();
    EXPECT_TRUE(controller->isHandRaised());
    EXPECT_EQ(spy.count(), 1);

    controller->toggleHandRaise();
    EXPECT_FALSE(controller->isHandRaised());
    EXPECT_EQ(spy.count(), 2);
}

TEST_F(MeetingControllerTest, ToggleRecording)
{
    QSignalSpy spy(controller, &MeetingController::recordingChanged);

    controller->toggleRecording();
    EXPECT_TRUE(controller->isRecording());

    controller->toggleRecording();
    EXPECT_FALSE(controller->isRecording());

    EXPECT_EQ(spy.count(), 2);
}

TEST_F(MeetingControllerTest, SetMicOnEmitsShowMessage)
{
    QSignalSpy msgSpy(controller, &MeetingController::showMessage);

    controller->setMicOn(true);

    EXPECT_GE(msgSpy.count(), 1);
    QString msg = msgSpy.last().at(0).toString();
    EXPECT_TRUE(msg.contains("麦克风"));
}

// ==================== 会议状态测试 ====================

TEST_F(MeetingControllerTest, SetInMeetingStartsTimer)
{
    QSignalSpy spy(controller, &MeetingController::inMeetingChanged);

    controller->setInMeeting(true);

    EXPECT_TRUE(controller->isInMeeting());
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(controller->meetingDuration(), "00:00");
}

TEST_F(MeetingControllerTest, SetInMeetingStopsTimer)
{
    controller->setInMeeting(true);

    QSignalSpy spy(controller, &MeetingController::inMeetingChanged);
    controller->setInMeeting(false);

    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(MeetingControllerTest, JoinMeetingWithEmptyId)
{
    QSignalSpy errorSpy(controller, &MeetingController::errorOccurred);

    controller->joinMeeting("");

    EXPECT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.first().at(0).toString().contains("请输入会议号"));
}

TEST_F(MeetingControllerTest, JoinMeetingSetsMeetingId)
{
    controller->setUserName("testuser");
    controller->joinMeeting("123456");

    EXPECT_EQ(controller->meetingId(), "123456");
    EXPECT_EQ(controller->meetingTitle(), "会议 123456");
}

TEST_F(MeetingControllerTest, CreateMeetingGeneratesId)
{
    controller->setUserName("张三");
    controller->createMeeting("团队周会");

    EXPECT_FALSE(controller->meetingId().isEmpty());
    EXPECT_EQ(controller->meetingTitle(), "团队周会");
}

TEST_F(MeetingControllerTest, CreateMeetingDefaultTitle)
{
    controller->setUserName("张三");
    controller->createMeeting("");

    EXPECT_EQ(controller->meetingTitle(), "张三的会议");
}

// ==================== 离开会议测试 ====================

TEST_F(MeetingControllerTest, LeaveMeetingResetsState)
{
    // 先设置一些状态
    controller->setUserName("testuser");
    controller->setInMeeting(true);
    controller->setMeetingId("123456");
    controller->setMeetingTitle("测试会议");
    controller->setScreenSharing(true);
    controller->setRecording(true);
    controller->setHandRaised(true);

    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);

    controller->leaveMeeting();

    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_FALSE(controller->isScreenSharing());
    EXPECT_FALSE(controller->isRecording());
    EXPECT_FALSE(controller->isHandRaised());
    EXPECT_FALSE(controller->isMicOn());
    EXPECT_FALSE(controller->isCameraOn());
    EXPECT_TRUE(controller->meetingId().isEmpty());
    EXPECT_TRUE(controller->meetingTitle().isEmpty());
    EXPECT_EQ(controller->participantCount(), 0);
    EXPECT_EQ(leftSpy.count(), 1);
}

TEST_F(MeetingControllerTest, EndMeetingCallsLeaveMeeting)
{
    controller->setInMeeting(true);

    QSignalSpy endSpy(controller, &MeetingController::meetingEnded);
    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);

    controller->endMeeting();

    EXPECT_EQ(endSpy.count(), 1);
    EXPECT_EQ(leftSpy.count(), 1);
    EXPECT_FALSE(controller->isInMeeting());
}

// ==================== 凭据保存测试 ====================

TEST_F(MeetingControllerTest, SaveAndLoadCredentials)
{
    controller->saveCredentials("testuser", "pass123");

    EXPECT_EQ(controller->getSavedUsername(), "testuser");
    EXPECT_EQ(controller->getSavedPassword(), "pass123");
    EXPECT_TRUE(controller->hasRememberedPassword());

    // 清理
    controller->clearSavedCredentials();

    EXPECT_TRUE(controller->getSavedUsername().isEmpty());
    EXPECT_TRUE(controller->getSavedPassword().isEmpty());
    EXPECT_FALSE(controller->hasRememberedPassword());
}

// ==================== 登出测试 ====================

TEST_F(MeetingControllerTest, LogoutClearsUsername)
{
    controller->setUserName("张三");
    QSignalSpy msgSpy(controller, &MeetingController::showMessage);

    controller->logout();

    EXPECT_TRUE(controller->userName().isEmpty());
    EXPECT_GE(msgSpy.count(), 1);
}

TEST_F(MeetingControllerTest, LogoutLeavesActiveMeeting)
{
    controller->setUserName("张三");
    controller->setInMeeting(true);

    QSignalSpy leftSpy(controller, &MeetingController::meetingLeft);

    controller->logout();

    EXPECT_FALSE(controller->isInMeeting());
    EXPECT_EQ(leftSpy.count(), 1);
}

// ==================== 会议时长格式化测试 ====================

TEST_F(MeetingControllerTest, MeetingDurationFormat)
{
    // 初始应为 "00:00"
    EXPECT_EQ(controller->meetingDuration(), "00:00");
}

// ==================== 连接状态测试 ====================

TEST_F(MeetingControllerTest, ConnectionStateReflectsLiveKit)
{
    // 未连接时
    EXPECT_FALSE(controller->isConnecting());
    EXPECT_FALSE(controller->isConnected());
}

// ==================== 复制会议信息测试 ====================

TEST_F(MeetingControllerTest, CopyMeetingInfo)
{
    controller->setMeetingId("123456789");

    QSignalSpy msgSpy(controller, &MeetingController::showMessage);
    controller->copyMeetingInfo();

    EXPECT_GE(msgSpy.count(), 1);
    EXPECT_TRUE(msgSpy.last().at(0).toString().contains("复制"));
}

// ==================== 视图切换测试 ====================

TEST_F(MeetingControllerTest, SwitchView)
{
    QSignalSpy msgSpy(controller, &MeetingController::showMessage);

    controller->switchView("grid");

    EXPECT_EQ(msgSpy.count(), 1);
    EXPECT_TRUE(msgSpy.first().at(0).toString().contains("grid"));
}
