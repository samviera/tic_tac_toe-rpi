#include "game.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <vector>

using namespace std;

Game::Game() {
  
}

//configgpio
void Game::init_gpio(GpioConfig (&gpio)[NUMBER_OF_GPIOS]) {
  //go through the array
  for (size_t i = 0; i < Game::NUMBER_OF_GPIOS; i++) {
    
    gpio_init(gpio[i].pin_number);
    gpio_set_dir(gpio[i].pin_number, gpio[i].pin_dir);
  }
}

bool Game::debounce(const volatile BtnState& btn) {
  if (has_changed(btn.prev_state, btn.curr_state)) {
    if (is_stable(btn.but_pin, btn.curr_state)) {
      return true;
    }
  }
  return false;
}

bool Game::is_stable(const uint button, const bool prev_state) {

  sleep_us(DEBOUNCE_DELAY);
  uint current_state = gpio_get(button);
  if (prev_state == HIGH && current_state == HIGH) {

#ifdef VERBOSE
    printf("Button state stable\n");
#endif
    return true;
  }
  return false;
}

//checking if changed

bool Game::has_changed(bool prev_state, bool curr_state) {
  bool changed = (prev_state == LOW && curr_state == HIGH);
  return changed;
}

void Game::update_btn_state(volatile BtnState *btn) {
 
  if (btn->curr_state == 0) {
  
    btn->prev_state = 0;
  }
  else if (btn->curr_state == 1) {
    btn->prev_state = 1;
  }
  btn->curr_state = gpio_get(btn->but_pin);
}
//starting over x starts empty col/row
void Game::reset_board(char *current_player, uint *moves, char (*board)[COLS],
                       bool *is_game_over) {
  cout << "Reset board" << endl;

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      //making them empty
      board[i][j] = EMPTY;
    }
  }

  *moves = 0;
  *current_player = X;
  *is_game_over = false;
  print_board(board);
  print_player_turn(*current_player);

  multicore_fifo_push_blocking(EMPTY);
}

uint Game::get_curr_row(const uint moves) {
 
  return static_cast<uint>(moves) / ROWS;
}

uint Game::get_next_row(const uint moves) {

  return static_cast<uint>(moves + 1) / ROWS;
}

uint Game::get_next_col(const uint moves) {
  
  return static_cast<uint>(moves + 1) % COLS;
}

uint Game::get_curr_col(const uint moves) {
 
  return static_cast<uint>(moves) % COLS;
}

void Game::update_position(uint *moves) {
  
  uint next_row = get_next_row(*moves);
  uint next_col = get_next_col(*moves);
  
  if (is_valid_pos(next_row, next_col)) {
    
    (*moves)++;
  } else {
    
#ifdef VERBOSE
  
    printf("End of board. Starting from top.\n");
#endif
    *moves = 0;
  }
}

void Game::print_curr_pos(const uint row, const uint col) {
  cout << "Row: " << row << " Col: " << col << endl;
}

bool Game::is_valid_pos(const uint row, const uint col) {

  return (row < ROWS && col < COLS);
}

bool Game::is_empty_pos(const uint row, const uint col,
                        const char (*board)[COLS]) {
  
  return (board[row][col] == EMPTY);
}

void Game::update_board(const char current_player, const uint moves,
                        char (*board)[COLS]) {
 
  uint row = get_curr_row(moves);

  uint col = get_curr_col(moves);

  cout << "Entering player " << current_player << " input into row " << row << " col " << col << endl;
  
  board[row][col] = current_player;
}

void Game::print_board(const char (*board)[COLS]) {
  // ANSI clear it and move to top 
  cout << "\e[1;1H\e[2J";
  //loop to not make more than one
  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      
      cout << " "  << board[i][j] << " ";
      
      if (j < COLS - 1) {
        cout << "|"; 
      }
    }
    if (i < ROWS - 1) {
      cout << endl << "---+---+---" << endl;
    } else {
      cout << endl;
    }
  }
}

void Game::print_player_turn(const char current_player) {
  cout << "Player " << current_player << " turn" << endl;
}

//player led basically light up red of X yellow if O
void Game::update_player_led(const char current_player) {
  if (current_player == X) {
  
    gpio_put(LED1, HIGH);
    gpio_put(LED2, LOW);
  }

  else if (current_player == O) {
  
    gpio_put(LED1, LOW);
    gpio_put(LED2, HIGH);
  }
  
  else {

    gpio_put(LED1, LOW);
    gpio_put(LED2, LOW);
  }
}

void Game::flash_winner_led() {
 
  uint32_t winner = static_cast<uint32_t>(EMPTY);
  uint led_pin = ONBOARD_LED;

  // super loop beginning
  while (true) {

    
    if (multicore_fifo_rvalid()) {
      winner = multicore_fifo_pop_blocking();
    }

    if ((char)winner == X) {
      led_pin = LED1;

    } else if ((char)winner == O) {
      led_pin = LED2;
    } else {
    
      led_pin = ONBOARD_LED;
    }
    
    gpio_put(led_pin, HIGH);
    sleep_ms(BLINK_LED_DELAY);
    gpio_put(led_pin, LOW);
    sleep_ms(BLINK_LED_DELAY);
  }
}

//button 1 moves the cursor
void Game::handle_btn1(uint *moves) {
  update_position(moves);
  uint curr_row = get_curr_row(*moves);
  uint curr_col = get_curr_col(*moves);
  print_curr_pos(curr_row, curr_col);
}

//button 2 selects
void Game::handle_btn2(char *current_player, uint *moves, char (*board)[COLS],bool *is_game_over) {
 
  uint row = get_curr_row(*moves);
  uint col = get_curr_col(*moves);

 //can we pick
  if (!is_valid_pos(row, col)) {
    printf("Invalid selection row %d col %d\n", row, col);
    return;
  }

//is it empty
  if (!is_empty_pos(row, col, board)) {
    printf("row %d col %d is taken.\n", row, col);
    printf("Pick another spot.\n");
    return;
  }
  update_board(*current_player, *moves, board);
  print_board(board);


  if (is_win(*current_player, board)) {
    //if they win
    printf("YAY Player %c wins!\n", *current_player);

    multicore_fifo_push_blocking(*current_player);

    *is_game_over = true;
    printf("\nPlease wait\n");
    printf("\nWaiting for the reset ...\n");
  } else if (is_tie(board)) {
    
    //if tie
    printf("Woah its a tie!\n");

    reset_board(current_player, moves, board, is_game_over);
  } else {
    
    *moves = 0;
    *current_player = get_new_player(*current_player);
    print_player_turn(*current_player);
  }
}

char Game::get_new_player(char current_player) {
  return (current_player == X) ? O : X;
}

bool Game::is_win(const char player, const char (*board)[COLS]) {
 //the way it checks to win is in these col and row
  for (int i = 0; i < ROWS; i++) {
    if (board[i][0] == player && board[i][1] == player &&
        board[i][2] == player) {
//if yes winner
      return true;
    }
  }
  //checks o
  for (int j = 0; j < COLS; j++) {
    if (board[0][j] == player && board[1][j] == player &&
        board[2][j] == player) {
//then o winner
      return true;
    }
  }
  //diag
  if (board[0][0] == player && board[1][1] == player && board[2][2] == player) {
    return true;
  }
  if (board[0][2] == player && board[1][1] == player && board[2][0] == player) {
    return true;
  }
  return false;
}

bool Game::is_tie(const char (*board)[COLS]) {

  for (int i = 0; i < ROWS; i++) {
    for (int j = 0; j < COLS; j++) {
      //checks if game is over or not basically if empty cell its not tied
      if (board[i][j] == EMPTY) {
        return false;
      }
    }
  }
 
  return true;
}
