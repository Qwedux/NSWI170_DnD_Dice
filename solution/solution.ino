#include "funshield.h"

constexpr int buttonPins[] { button1_pin, button2_pin, button3_pin };
constexpr int buttonPinsCount = sizeof(buttonPins) / sizeof(buttonPins[0]);

constexpr int no_of_glyphs = 4;
constexpr int dice_types[] {4, 6, 10, 12, 20, 100};
constexpr byte segmentMap[] = {
    0xC0, // 0  0b11000000
    0xF9, // 1  0b11111001
    0xA4, // 2  0b10100100
    0xB0, // 3  0b10110000
    0x99, // 4  0b10011001
    0x92, // 5  0b10010010
    0x82, // 6  0b10000010
    0xF8, // 7  0b11111000
    0x80, // 8  0b10000000
    0x90, // 9  0b10010000
    0xA1  // d  0b10100001
};

struct configuration{
  size_t index_of_current_dice;
  unsigned int dice_rolls;
  configuration(int no_of_rolls, size_t which_dice){
    dice_rolls = no_of_rolls;
    index_of_current_dice = which_dice;
  }
  configuration(){
    dice_rolls = 1;
    index_of_current_dice = 0;
  }
  void increase_dice_rolls(){
    dice_rolls = dice_rolls == 9 ? 1 : (dice_rolls + 1) % 10;
  }
  unsigned int get_dice_rolls(){
    return dice_rolls;
  }
  void next_dice(){
    index_of_current_dice = (index_of_current_dice + 1) % (sizeof(dice_types) / sizeof(dice_types[0]));
  }
  int get_current_dice(){
    return dice_types[index_of_current_dice];
  }
};

struct seg_display {
  int order; //which digit is currently displayed
  byte displayed_glyphs[no_of_glyphs];
  void convert_conf_to_glyphs(configuration conf){
    displayed_glyphs[0] = segmentMap[conf.get_dice_rolls()];
    displayed_glyphs[1] = segmentMap[10];
    displayed_glyphs[2] = segmentMap[(conf.get_current_dice()/10) % 10];
    displayed_glyphs[3] = segmentMap[conf.get_current_dice() % 10];
  }
  void convert_generated_to_glyphs(int number){
    for(int position = no_of_glyphs-1; position >= 0; position--){
      if(number > 0){
        displayed_glyphs[position] = segmentMap[number % 10];
        number /= 10;
      }
      else {displayed_glyphs[position] = 0xFF;}
    }
  }
  seg_display(configuration conf){
    for(int i = 0; i < no_of_glyphs; i++){
      displayed_glyphs[i] = 0xFF;
    }
    order = no_of_glyphs - 1;
    convert_conf_to_glyphs(conf);
  }
  void change_displayed_digit(){
    order = ((order-1) + no_of_glyphs) % no_of_glyphs;
  }
  void write_glyph() {
    byte position = digit_muxpos[order];
    byte glyph = displayed_glyphs[order];
    digitalWrite(latch_pin, LOW);
    shiftOut(data_pin, clock_pin, MSBFIRST, glyph);
    shiftOut(data_pin, clock_pin, MSBFIRST, position);
    digitalWrite(latch_pin, HIGH);
  }
};

int line_break = 0;

struct generator_of_randomness{
  int get_result_of_dice_rolls(configuration conf){
    unsigned long long result = 0;
    randomSeed(millis());
    for(unsigned int repeats = 0; repeats < conf.get_dice_rolls(); repeats++){
      unsigned long long a = ((random(conf.get_current_dice())+conf.get_current_dice()) % conf.get_current_dice()) + 1;
      result +=  a;
    }
    // Serial.print((int)result);
    // Serial.print(", ");
    // line_break++;
    // if(line_break == 40){
    //   line_break = 0;
    //   Serial.print("\n");
    // }
    return result;
  }
};

struct button{
  int Pin;
  bool is_pressed;
  button (int set_pin_to){
    Pin = set_pin_to;
    is_pressed = 0;
  }
  void change_state(){
    is_pressed = !is_pressed;
  }
  bool get_state(){
    return is_pressed;
  }
  bool is_no_longer_pressed(){
    return digitalRead(Pin) == OFF && get_state() == 1;
  }
  bool is_newly_pressed(){
    return digitalRead(Pin) == ON && get_state() == 0;
  }
  bool is_held(){
    return digitalRead(Pin) == ON && get_state() == 1;
  }
};

enum class modes{
  NORMAL,
  CONFIGURATION
};

button buttons[] {{button1_pin}, {button2_pin}, {button3_pin}};
configuration settings;
seg_display displej(settings);
generator_of_randomness generator;
unsigned long time;
constexpr unsigned long when_to_get_new_number = 40;
modes mode = modes::CONFIGURATION;

void setup() {
  if(no_of_glyphs < 4){
    return;
  }
  for (int i = 0; i < buttonPinsCount; ++i) {
    pinMode(buttons[i].Pin, INPUT);
  }
  pinMode(latch_pin, OUTPUT);
  pinMode(clock_pin, OUTPUT);
  pinMode(data_pin, OUTPUT);
  // Serial.begin(9600);
}

void keep_generating(){
  if (time + when_to_get_new_number < millis()){
    displej.convert_generated_to_glyphs(generator.get_result_of_dice_rolls(settings));
    time = millis();
  }
}
void loop() {
  if(no_of_glyphs < 4){
    return;
  }
  displej.write_glyph();
  displej.change_displayed_digit();
  if(buttons[0].is_newly_pressed()){
    buttons[0].change_state();
    mode = modes::NORMAL;
    displej.convert_generated_to_glyphs(generator.get_result_of_dice_rolls(settings));
    time = millis();
  }
  else if(buttons[0].is_held()){
    keep_generating();
  }
  else if(buttons[0].is_no_longer_pressed()){
    buttons[0].change_state();
  }
  else if(buttons[1].is_newly_pressed()){
    if(mode == modes::NORMAL){
      mode = modes::CONFIGURATION;
    }
    else{
      settings.increase_dice_rolls();
    }
    displej.convert_conf_to_glyphs(settings);
    buttons[1].change_state();
  }
  else if(buttons[1].is_no_longer_pressed()){
    buttons[1].change_state();
  }
  else if(buttons[2].is_newly_pressed()){
    if(mode == modes::NORMAL){
      mode = modes::CONFIGURATION;
    }
    else{
      settings.next_dice();
    }
    displej.convert_conf_to_glyphs(settings);
    buttons[2].change_state();
  }
  else if(buttons[2].is_no_longer_pressed()){
    buttons[2].change_state();
  }
}
