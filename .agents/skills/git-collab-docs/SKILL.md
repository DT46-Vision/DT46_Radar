---
name: git-collab-docs
description: 管理协作仓库的分支、提交和合并。提交说明只写做了什么。用于用户提到提交、commit、push、pull、分支、PR、合并、初始化仓库时。
---

# Git 协作

不对 `main` 使用 `push --force`。不改 `git config`，不用交互式 rebase。作者和时间用 git 元数据，不写进说明。

## 初始化

新库默认主分支 `main`。`git init -b main`。本目录如果还不是 Git 仓库，先问用户要不要初始化，不擅自 `git init`。

## 先判

- 没有 remote、或用户说只在本机：只做本地分支和提交，不要求 push。
- 已经和远程协作：`main` 上不直接 commit。任何改动从更新后的 `origin/main` 开分支。
- 保护路径：`AGENTS.md`、`CLAUDE.md`、`.claude/`、`.agents/`。默认只读；用户点名才改，并且不和业务代码捆在同一次提交里。
- 不提交密钥、`.env`、点云、录像、IQ 录波和构建目录。

## 分支和提交

修问题、做功能、改行为：`fix/`、`feat/`、`opt/` 加短名。

提交第一行要让没做这件事的人看出现在多了什么或修了什么。用户没点名不 push。

## 合并

事情做完，并且用户认为已经验证过，才讨论合并。合并前再确认一次。未确认不 merge。

```text
git checkout main
git pull
git merge feat/短名
git push
```

仅本地仓库不做 pull 和 push。
