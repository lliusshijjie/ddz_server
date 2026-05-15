CREATE TABLE IF NOT EXISTS player (
  player_id BIGINT NOT NULL PRIMARY KEY,
  coin BIGINT NOT NULL,
  updated_at_ms BIGINT NOT NULL
);

CREATE TABLE IF NOT EXISTS game_record (
  record_id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  room_id BIGINT NOT NULL,
  game_mode INT NOT NULL,
  winner_player_id BIGINT NOT NULL,
  started_at_ms BIGINT NOT NULL,
  ended_at_ms BIGINT NOT NULL,
  players_csv VARCHAR(255) NOT NULL
);

CREATE TABLE IF NOT EXISTS coin_log (
  id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,
  player_id BIGINT NOT NULL,
  room_id BIGINT NOT NULL,
  change_amount BIGINT NOT NULL,
  before_coin BIGINT NOT NULL,
  after_coin BIGINT NOT NULL,
  created_at_ms BIGINT NOT NULL
);
