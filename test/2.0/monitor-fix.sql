-- phpMyAdmin SQL Dump
-- version 4.5.4.1deb2ubuntu2
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: 2017-04-15 15:10:31
-- 服务器版本： 5.7.17-0ubuntu0.16.04.2
-- PHP Version: 7.0.15-0ubuntu0.16.04.4

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `matrix_monitor`
--

-- --------------------------------------------------------

--
-- 表的结构 `judge_worker_status`
--

DROP TABLE IF EXISTS `judge_worker_status`;
CREATE TABLE `judge_worker_status` (
  `host` varchar(30) NOT NULL,
  `worker_id` int(4) NOT NULL,
  `worker_type` varchar(30) NOT NULL,
  `is_running` tinyint(1) NOT NULL DEFAULT '0',
  `worker_stage` varchar(20) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `judge_worker_status`
--
ALTER TABLE `judge_worker_status`
  ADD PRIMARY KEY (`host`,`worker_id`),
  ADD KEY `node_id` (`host`,`worker_id`);

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;


-- phpMyAdmin SQL Dump
-- version 4.5.4.1deb2ubuntu2
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: 2017-04-15 15:10:06
-- 服务器版本： 5.7.17-0ubuntu0.16.04.2
-- PHP Version: 7.0.15-0ubuntu0.16.04.4

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `matrix_monitor`
--

-- --------------------------------------------------------

--
-- 表的结构 `judge_node_config`
--

DROP TABLE IF EXISTS `judge_node_config`;
CREATE TABLE `judge_node_config` (
  `host` varchar(30) NOT NULL,
  `port` int(4) NOT NULL,
  `thread_number` int(4) NOT NULL DEFAULT '0',
  `current_programming_task` int(4) NOT NULL DEFAULT '0',
  `current_choice_task` int(4) NOT NULL DEFAULT '0',
  `current_output_task` int(4) NOT NULL DEFAULT '0',
  `current_programBlankFilling_task` int(11) NOT NULL DEFAULT '0',
  `downloading_task` int(4) NOT NULL DEFAULT '0',
  `is_working` int(4) NOT NULL DEFAULT '0',
  `load_factor` int(4) NOT NULL,
  `total_node_number` int(4) NOT NULL DEFAULT '10'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `judge_node_config`
--
ALTER TABLE `judge_node_config`
  ADD PRIMARY KEY (`host`);

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
