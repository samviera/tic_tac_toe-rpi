#include "game.hpp"

int main() {

  char board[Game::ROWS][Game::COLS] = {
      {Game::EMPTY, Game::EMPTY, Game::EMPTY},
      {Game::EMPTY, Game::EMPTY, Game::EMPTY},
      {Game::EMPTY, Game::EMPTY, Game::EMPTY}};

 
  char current_player = Game::X;
  uint moves = 0;

  bool is_game_over = false;

  //GPIO configurations
  GpioConfig my_gpio[Game::NUMBER_OF_GPIOS] = {
      {Game::LED1, GPIO_OUT}, {Game::LED2, GPIO_OUT},
      {Game::BTN1, GPIO_IN},  {Game::BTN2, GPIO_IN},
      {Game::BTN3, GPIO_IN},  {Game::ONBOARD_LED, GPIO_OUT}};

  // volatile not cache but from hardware
  volatile BtnState btn1 = {
      .but_pin = Game::BTN1, 
      .prev_state = false,
      .curr_state = false};

  volatile BtnState btn2 = {
      .but_pin = Game::BTN2,
      .prev_state = false,
      .curr_state = false};

  volatile BtnState btn3 = {
      .but_pin = Game::BTN3, 
      .prev_state = false,
      .curr_state = false};

  Game game; 

  
  stdio_init_all();
  multicore_launch_core1(Game::flash_winner_led);

  game.init_gpio(my_gpio);

 //start over
  game.reset_board(&current_player, &moves, board, &is_game_over);

  while (true) {
    //if the game isnt over update led and btn state 
    if (!is_game_over) {
      game.update_player_led(current_player);

      
      game.update_btn_state(&btn1);
      if (game.debounce(btn1)) {
        game.handle_btn1(&moves);
      }

     
      game.update_btn_state(&btn2);
      if (game.debounce(btn2)) {
        game.handle_btn2(&current_player, &moves, board, &is_game_over);
      }
    }

   
    game.update_btn_state(&btn3);
    if (game.debounce(btn3)) {
      game.reset_board(&current_player, &moves, board, &is_game_over);
    }
  }

  return 0;
}
