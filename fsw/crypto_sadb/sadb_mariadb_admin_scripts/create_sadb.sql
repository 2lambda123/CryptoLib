CREATE DATABASE IF NOT EXISTS sadb;

USE sadb;

-- IV_LEN should probably not have that default -- to be reviewed.

CREATE TABLE security_associations
(
  spi INT NOT NULL
  ,ekid MEDIUMINT NOT NULL DEFAULT spi
  ,akid MEDIUMINT NOT NULL DEFAULT spi
  ,sa_state SMALLINT NOT NULL DEFAULT 0
  ,tfvn TINYINT
  ,scid SMALLINT
  ,vcid TINYINT
  ,mapid TINYINT
  ,lpid SMALLINT
  ,est SMALLINT
  ,ast SMALLINT
  ,shivf_len SMALLINT
  ,shsnf_len SMALLINT
  ,shplf_len SMALLINT
  ,stmacf_len SMALLINT
  ,ecs_len SMALLINT
  ,ecs BINARY(4) NOT NULL DEFAULT X'00000000' -- ECS_SIZE=4
  ,iv_len SMALLINT NOT NULL DEFAULT 12
  ,iv BINARY(12) NOT NULL DEFAULT X'000000000000000000000000' -- IV_SIZE=12
  ,acs_len SMALLINT NOT NULL DEFAULT 0
  ,acs SMALLINT NOT NULL DEFAULT 0
  ,abm_len MEDIUMINT
  ,abm BINARY(20) NOT NULL DEFAULT X'1111111111111111111111111111111111111111' -- ABM_SIZE=20
  ,arc_len SMALLINT NOT NULL DEFAULT 0
  ,arc BINARY(20) NOT NULL DEFAULT X'0000000000000000000000000000000000000000' -- ARC_SIZE=20 , TBD why so large...
  ,arcw_len SMALLINT
  ,arcw BINARY(1) NOT NULL DEFAULT X'00' -- ARCW_SIZE=1
);