# GitHub Pull Request (PR) 标准工程化工作流指南

本文档总结了在团队协作或开源项目中，使用基于分支和 Pull Request 的标准代码提交流程。

## 核心概念：什么是 Pull Request (PR)？
Pull Request 是一种合并请求机制。当你开发完某个功能或修复某个 Bug 后，请求将你**新开发的代码分支**合并到**主代码库 (通常是 `main` 或 `master` 分支)** 中。它提供了一个代码审查 (Code Review) 和讨论的可视化平台，确保只有经过验证和团队成员同意的高质量代码才能进入主干。

---

## 阶段一：本地开发与代码提交

### 1. 同步最新的主干代码
在开始任何新功能的开发前，始终确保你本地的主分支是最新的，避免后续发生代码冲突。
```bash
git checkout main
git pull origin main
```

### 2. 创建并切换到功能分支
**原则：永远不要直接在 `main` 这种主分支上直接写代码和提交。** 针对每个新功能或 Bug 修复，创建一个专属的开发分支。
```bash
# -b 代表 branch，意为“创建并立刻切换到该新分支”
git checkout -b feature/your-feature-name

git checkout -b feature/login 等同于你敲了两行命令：
git branch feature/login （创建分支）
git checkout feature/login （切换到该分支）

# 分支命名规范建议：
# 功能开发：feature/xxx 
# 修复Bug：fix/xxx
# 文档修改：docs/xxx
# 性能优化：perf/xxx
```

### 3. 本地编写代码并提交
在 IDE 或编辑器中完成你的开发工作。完成后，将更改保存到本地 Git 仓库。
```bash
# 查看有哪些文件被修改
git status 

# 将所有改动加入暂存区（. 代表当前目录下所有改动）
git add . 

# 提交改动，并添加清晰的描述信息
git commit -m "feat: 添加了XXX功能" 
```

### 4. 推送分支到远程 GitHub 仓库
将这个你在本地全新建好的分支，推送到远端的 GitHub 上。
```bash
# 第一次推送新分支时必须加上 -u
# -u 代表 --set-upstream，它的作用是把“本地的新分支”和“远程 GitHub 上的同名新分支”绑定起来。
git push -u origin feature/your-feature-name

# 绑定之后，以后如果在这个分支上还要提交新代码，只需要简单运行：
# git push
```

---

## 阶段二：在 GitHub 上发起 PR 请求

1. **进入 GitHub 仓库页面**：刚完成上一步的推包后，打开浏览器进入该项目的 GitHub 页面。
2. **点击绿色横幅**：由于检测到新推的分支，页面顶部通常会自动弹出一条亮绿色横幅提示。直接点击上面的 **"Compare & pull request"** 按钮。
   *(如果没看到提示，亦可点击 "Pull requests" 标签页 -> 点击 "New pull request" 按钮，手动将你的新分支对比 `main` 分支。)*
3. **填写 PR 信息**：
   - **Title (标题)**：简明扼要说明该 PR 要做什么（例：修复音频切换时的杂音问题）。
   - **Description (描述)**：详细写明本次更改的背景内容、测试方式或截图，以便审阅者快速理解。
   - **Reviewers (指派审阅人)**：在右侧指派团队中的其他成员来审查这段代码。
4. **提交创建**：点击下方绿色的 **"Create pull request"** 按钮。

---

## 阶段三：代码审阅 (Code Review) 与迭代修改

PR 建立后，被指派的队友会进去看你改了哪些代码。如果他们觉得某段逻辑需要修改，会在 PR 里给你留言或 "Request changes"。

接到修改意见后，你需要**继续修改代码**。**不用撤销 PR，也不用新建分支**，直接在刚才那个分支上改：
```bash
# 1. 在本地编辑器里修改不完善的代码
# 2. 存入暂存区：
git add .
# 3. 提交一个修复 commit：
git commit -m "fix: 根据 review 意见调整了初始化逻辑"
# 4. 直接推送（因为之前用过 -u，这里直接 push 即可）：
git push origin feature/your-feature-name
```
> **提示**：只要 PR 还没有被合并或关闭，你往这个分支推的任何新提交，都会实时地自动追加在那个 PR 页面里。

---

## 阶段四：合并 (Merge) 与本地清理

经过讨论调整，大家觉得代码完美了，审阅者点了 "Approve" (批准通过)。

### 1. 网页端合并代码
1. 滚动到 GitHub 此 PR 页面的最下方，点击绿色的 **"Merge pull request"** 按钮。
2. 点击 **"Confirm merge"** (确认合并)。
3. *(强烈建议)* 点击接下来出现的 **"Delete branch"** 按钮，把 GitHub 上这个已经合并完的历史功能分支删掉，保持远程仓库干干净净。

### 2. 同步与清理本地代码库
此时远端的 `main` 已经包含了你的新代码。你要让本地的 `main` 也保持同步，并把废弃的本地分支清理掉：
```bash
# 切换回主分支
git checkout main

# 将远端包含你新代码的最新 main 拉取到本地
git pull origin main

# 安全删除本地的那个功能分支（因为它已经完成了使命）
git branch -d feature/your-feature-name
```

---
**至此，一个标准且极其优雅的 GitHub 代码协作全过程就结束了！**

---

## 附加技能：如何使用 Git 打标签 (Tag)

在软件开发中，**标签 (Tag)** 通常被用来标记重要的里程碑（比如发布正式版本：`v1.0.0`, `v2.1.3` 等）。打标签就相当于给某次特定的 commit 拍了一张带有名字的快照。

### 1. 本地创建标签

Git 的标签分为两种：轻量标签 (Lightweight) 和附注标签 (Annotated)。**强烈推荐使用附注标签**，因为它包含了打标签者的名字、电子邮件、日期，还可以添加标签说明。

```bash
# 方法一：创建附注标签 (推荐)
# -a 代表 annotated (附注形式)
# -m 后面跟着的是对这个标签的说明文字
git tag -a v1.0.0 -m "发布 1.0.0 首个正式版本"

# 方法二：创建轻量标签 (不推荐用于正式发布，通常用于本地临时标记)
git tag v1.0.0-beta
```

> **注意：** 默认情况下，`git tag` 是对你**当前所在的提交 (Commit)** 打标签。如果你想给过去某次历史提交打标签，可以在结尾加上该 commit 的哈希串：
> `git tag -a v0.9.0 9fceb02 -m "补打 0.9.0 版本的标签"`

### 2. 查看标签

你可以列出本地现有的所有标签，或者查看某个标签的具体信息。

```bash
# 列出所有标签
git tag

# 查找带特定字符的标签
git tag -l "v1.0.*"

# 查看某个标签的具体信息 (会显示是哪次 commit、是谁打的以及备注信息)
git show v1.0.0
```

### 3. 将标签推送到远端 GitHub

**非常重要：** 普通的 `git push` 命令是**不会**把标签推送到远程服务器上的！你必须明确地告诉 Git 推送标签。

```bash
# 只推送某一个特定的标签到远端
git push origin v1.0.0

# 一次性推送所有本地未推送到远端的标签 (如果你打了很多个)
git push origin --tags
```

*(推送到 GitHub 后，项目的网页右侧 `Releases / Tags` 栏就会亮起，别人就能方便地下载你打包好的这个版本源码啦。)*

### 4. 删除标签

如果你不小心写错名字，或者想撤销某个标签，分为本地删除和远端删除两步。

```bash
# 1. 先删除本地的标签
# -d 代表 delete
git tag -d v1.0.0

# 2. 如果标签已经推到了 GitHub，你还需要执行下面的命令把远端的删掉：
# (注意写法很奇特，是将一个:推给远程标签，也就是相当于删除它)
git push origin :refs/tags/v1.0.0 
# 或在较新版本的 Git 中使用更直观的：
# git push origin --delete v1.0.0
```
