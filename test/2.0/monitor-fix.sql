--- matrix_monitor

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";

DROP TABLE IF EXISTS `judge_worker_status`;
CREATE TABLE `judge_worker_status` (
  `host` varchar(30) NOT NULL,
  `worker_id` int(4) NOT NULL,
  `worker_type` varchar(30) NOT NULL,
  `is_running` tinyint(1) NOT NULL DEFAULT '0',
  `worker_stage` varchar(20) DEFAULT NULL,
  PRIMARY KEY (`host`, `worker_id`),
  KEY `node_id` (`host`, `worker_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

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
  `total_node_number` int(4) NOT NULL DEFAULT '10',
  PRIMARY KEY (`host`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
