# judge-system 4.0
一个支持 DOMjudge、Sicily Online Judge、Matrix Online Judge 的评测系统。
本评测系统分为两个项目：judge-system、runguard。其中 runguard 子项目的细节请到 runguard 文件夹查看。

## 架构
一台机器运行一个 daemon，每个 daemon 可以有一些 runner，runner 可以执行不同的任务：

* compilation_task：负责执行编译，编译完成后的文件允许各个 runner 评测时读取（通过 overlayfs 来只读挂载）
* testcase_task：负责测试。支持如下的测试类型：
    * 标准测试：有设计好的输入输出，由 special judge/diff 来对程序输出进行比较。
    * 随机测试：出题人提供输入数据的随机生成程序，在每次评测时调用该程序生成输入和输出，再进行标准测试。由于每次进行随机测试都要先调用出题人的数据生成器和标准程序，因此执行速度也比较慢。
    * 静态测试：静态测试选手程序是否存在问题，如存在漏洞。
    * 内存测试：内存测试选手程序是否有内存泄漏情况，通常速度比较慢。
    * 单元测试：出题人提供单元测试对选手程序进行判分。
对于测试类型，目前的实现机制是通过硬编码实现的：不同评测类型的评测代码硬编码在 C++ 代码中，而没有通过脚本外置实现热替换。日后的开发应当考虑将此部分通用化。难点是每种测试的信息都不一致，可能需要 json 序列化信息后传送给外置的 python 脚本解析后评测并计算得分。

由于考试题目的类型有别，我们区分情况进行讨论：

### OI 赛制题目
广义上的 OI 赛制题目是：有部分分。也就是说一道题的所有测试点都必须进行评测，这种情况下如果评测系统的负载比较小，一个人的评测速度也可能因为打暴力而导致评测速度较慢。因此我们采取不同测试点不同核心运行的策略：一道题，10 个测试点，由不同的 test-runner 负责进行评测。这样的好处就是评测系统负载低时一个人的测试速度可以很快。

优化：对于标准测试，题目的比较器 special_judge 需要在评测前完成编译，由于编译速度比较慢，会拖慢标准测试时间，因此我们要做一个缓存来存储 special_judge 的可执行文件。文件系统将允许手动添加缓存文件，并管理依赖，special_judge 的可执行文件生成后添加进本地缓存系统。此外，随机生成的数据也必须添加进本地缓存系统。所有的 runner 共享一个缓存系统。

## 代码
本项目的目录树如下：
```
.
├── runguard // runguard 子项目
│   ├── include // 头文件
│   └── src // 源文件
├── src // judge-system 项目源文件
│   ├── server // daemon 的源代码
│   │   ├── domjudge // DOMjudge 的对接代码
│   │   ├── moj // judge-system 3.0 的对接代码
│   │   ├── mcourse // judge-system 2.0 的对接代码
│   │   └── sicily // Sicily OJ 的对接代码
│   └── client // runner 的源代码
└── include // judge-system 项目头文件
```

## 运行目录结构
本项目运行时需要提供
* EXEC_DIR: 备用的 executable 本地目录，如果没有服务器能提供 executable，那么从这里根据 id 找可用的
    ```
    EXEC_DIR
    ├── check // 测试执行的脚本
    │   ├── standard // 标准和随机测试的辅助测试脚本
    │   ├── static // 静态测试脚本
    │   └── memory // 内存检查（输入数据标准或随机）的帮助脚本
    ├── compare // 比较脚本
    │   ├── diff-all // 精确比较，如果有空白字符差异则返回 PE
    │   └── diff-ign-space // 忽略行末空格和文末空行的比较脚本，不会有 PE
    ├── compile // 编译脚本
    │   ├── c // C 语言程序编译脚本
    │   ├── cpp // C++ 语言程序编译脚本
    │   ├── make // Makefile 编译脚本
    │   ├── cmake // CMake 编译脚本
    │   └── ...
    └── run // 运行脚本
        └── standard // 标准运行脚本
    ```
* CACHE_DIR： 缓存目录，会缓存所有编译好的随机数据生成器、标准程序、生成的随机数据。
    ```
    CACHE_DIR
    ├── sicily // category id
    │   └── 1001 // problem id
    │       ├── standard // 标准程序的缓存目录（代码和可执行文件）
    │       ├── standard_data // 标准输入输出数据的缓存目录
    │       ├── compare // 比较器
    │       ├── random // 随机数据生成器的缓存目录（代码和可执行文件）
    │       └── random_data // 随机输入输出数据的缓存目录
    ├── moj
    └── mcourse
    ```
* DATA_DIR：数据缓存目录，如果设置了拷贝数据选项，那么评测系统将在 CACHE_DIR 内存储的数据拷贝到 DATA_DIR 中保存，如果将 DATA_DIR 放进内存盘将可以加速选手程序的 IO 性能，避免 IO 瓶颈
    ```
    DATA_DIR
    ├── ABCDEFG // 随机生成的 uuid
    │   ├── input // 当前测试数据组的输入数据文件夹
    │   └── output // 当前测试数据组的输出数据文件夹
    └── ...
    ```
* RUN_DIR: 运行目录，该目录会临时存储所有的选手程序代码、选手程序的编译运行结果
    ```
    RUN_DIR
    ├── sicily // category id
    │   └── 1001 // problem id
    │       └── 5100001 // submission id, workdir
    │           ├── compile // 选手程序的代码和编译目录
    │           │   ├── main.cpp // 选手程序的主代码（示例）
    │           │   ├── run // 选手程序的执行入口（可能是生成的运行脚本）
    │           │   ├── compile.out // 编译器的输出
    │           │   ├── compile.meta // 编译器的运行信息
    │           │   └── (Makefile) // 允许 Makefile
    │           ├── run-[client_id] // 选手程序的运行目录，包含选手程序输出结果
    │           │   ├── run // 运行路径
    │           │   │   ├── testdata.in // 映射进来的标准输入文件
    │           │   │   ├── testdata.out // 映射进来的标准输出文件（在选手程序运行完成后再挂载）
    │           │   │   └── program.out // 选手程序的 stdout 输出
    │           │   ├── feedback // 比较器结果路径
    │           │   │   ├── judgemessage.txt // 比较器的输出信息
    │           │   │   ├── judgeerror.txt // 比较器的错误信息
    │           │   │   └── score.txt // 如果比较器支持部分分，那么这里将保存部分分数
    │           │   ├── work // 构建的根目录
    │           │   ├── program.err // 选手程序的 stderr 输出
    │           │   ├── program.meta // 选手程序的运行信息
    │           │   └── runguard.err // runguard 的错误输出
    │           └── run-...
    ├── moj
    └── mcourse
    ```
* CHROOT_DIR: debootstrap 产生的 Linux 子系统环境。
    ```
    CHROOT_DIR
    ├── usr
    ├── bin
    └── ...
    ```

## 部署
### 配置 chroot
无论是评测系统本身，还是选手程序，都需要 chroot 环境来模拟一个干净的 Linux 执行环境，我们通过 debootstrap 来产生一个完整的 Ubuntu 发行版环境。这有两个好处：获得的干净的 Linux 执行环境允许进行编译、运行操作，还可以支持 bash 脚本作为被评测的程序；允许评测系统和选手程序运行在和宿主机不同的发行版本上：比如宿主机是 Ubuntu 16.04 LTS，评测系统和选手程序可以运行在 Ubuntu 19.04 上（尽管内核版本必须和宿主机一致），这样我们就可以通过 apt 来获取最新的软件包，支持各大语言的最新编译环境和运行时，而且省了更换宿主机操作系统的麻烦。
以上的两个好处都同样能从 docker 获得。

一个 `chroot` 环境需要包含：

完整的 `Ubuntu` 环境：以允许 bash 脚本的运行。
各个编程语言的编译运行环境：
* curl
* golang
* rustc
* default-jdk-headless
* pypy
* python3
* clang
* ruby
* scala
* libboost-all-dev（支持评测系统、runguard 的运行时环境、允许选手调用 boost 库）
* cmake（支持题目使用 cmake 编译）
* libgtest-dev（支持 gtest 评测）
* gcc, gcc-10, g++, g++-10（允许使用 C++2a）
* gcc-multilib, g++-multilib（评测的 spj 可能是 32 位的）
* fp-compiler
* valgrind（支持内存检查）。


### 部署方案
部署采用 systemd 方式部署。评测系统使用了 overlayfs 和 cgroup、命名空间来对选手程序进行权限控制，其实是借鉴了 docker 所使用的虚拟化技术。允许通过命令行参数来对核心使用进行控制：
```bash
./judge-system --enable-sicily=/etc/judge-system/sicily.conf --enable-3=/etc/judge-system/moj.conf --enable-2=/etc/judge-system/mcourse.conf --enable-2=/etc/judge-system/mexam.conf --server=0 --client=1-4 --clients=15 --exec-dir=/opt/judge-system/exec --cache-dir=/tmp/judge-system --run-dir=/ramdisk/judge-system --chroot-dir=/chroot
```

由于评测系统可以直接通过系统服务部署，你同样可以构建 docker 镜像来一键部署评测系统（虽然我不推荐这么做，这样会使得选手程序的运行效率减慢 20%，降低评测速度）。
为了减轻一台服务器 20 个评测队列一起抢 IO 从而导致评测结果不准确，我们使用内存盘来确保 IO 性能：程序的输入输出的 IO 操作全部在内存中完成，内存的速度显然比磁盘 IO 快，就算这导致了内存带宽的不足，也会比多核心抢 IO 要来的好；其次，选手程序是临时文件，并不需要写入磁盘，这样能减少评测系统对磁盘的消耗。

评测系统必要时也可以运行在配置好的 chroot 环境下以避免 boost 的编译。