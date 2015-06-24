#include <pebble.h>
  
#define KEY_TEMPERATURE 0
#define KEY_CONDITIONS 1
  
static Window *s_main_window;
static TextLayer *s_timedate_layer;
static TextLayer *s_weather_layer;

static GFont s_font;

static GBitmap *s_background_bitmap = NULL;
static BitmapLayer *s_background_layer;

#define NUMBER_OF_IMAGES 10
static int8_t image_index = 0;
const int IMAGE_RESOURCE_IDS[NUMBER_OF_IMAGES] = {
  RESOURCE_ID_IMAGE_01,
  RESOURCE_ID_IMAGE_02,
  RESOURCE_ID_IMAGE_03,
  RESOURCE_ID_IMAGE_04,
  RESOURCE_ID_IMAGE_05,
  RESOURCE_ID_IMAGE_06,
  RESOURCE_ID_IMAGE_07,
  RESOURCE_ID_IMAGE_08,
  RESOURCE_ID_IMAGE_09,
  RESOURCE_ID_IMAGE_10
};

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[] = "00:00 // mm.dd";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    //Use 2h hour format
    strftime(buffer, sizeof("00:00 // mm.dd"), "%H:%M // %m.%d", tick_time);
  } else {
    //Use 12 hour format
    strftime(buffer, sizeof("00:00 // mm.dd"), "%I:%M // %m.%d", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_timedate_layer, buffer);
}

static void main_window_load(Window *window) {
  s_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_PROTOTYPE_18));
  
  // Create GBitmap, then set to created BitmapLayer
  s_background_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_02);
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_background_layer));
  
  // Create time TextLayer
  s_timedate_layer = text_layer_create(GRect(0, 2, 144, 25));
  text_layer_set_background_color(s_timedate_layer, GColorWhite);
  text_layer_set_text_color(s_timedate_layer, GColorBlack);
  text_layer_set_text_alignment(s_timedate_layer, GTextAlignmentCenter);
  text_layer_set_text(s_timedate_layer, "00:00 // mm.dd");
  text_layer_set_font(s_timedate_layer, s_font);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_timedate_layer));
  
  // Create temperature Layer
  s_weather_layer = text_layer_create(GRect(0, 140, 144, 25));
  text_layer_set_background_color(s_weather_layer, GColorWhite);
  text_layer_set_text_color(s_weather_layer, GColorBlack);
  text_layer_set_text_alignment(s_weather_layer, GTextAlignmentCenter);
  text_layer_set_text(s_weather_layer, "Loading...");
  
  // Create second custom font, apply it and add to Window
  text_layer_set_font(s_weather_layer, s_font);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_weather_layer));
  
  // Make sure the time is displayed from the start
  update_time();
}

static void main_window_unload(Window *window) {
  fonts_unload_custom_font(s_font);
  gbitmap_destroy(s_background_bitmap);
  bitmap_layer_destroy(s_background_layer);
  text_layer_destroy(s_timedate_layer);
  text_layer_destroy(s_weather_layer);
}

static void update_background() {
  image_index++;
  if (image_index == NUMBER_OF_IMAGES) {
    image_index = 0;
  }
  
  if (s_background_bitmap != NULL) {
    gbitmap_destroy(s_background_bitmap);
    layer_remove_from_parent(bitmap_layer_get_layer(s_background_layer));
    bitmap_layer_destroy(s_background_layer);
  }
  
  s_background_bitmap = gbitmap_create_with_resource(IMAGE_RESOURCE_IDS[image_index]);
  s_background_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_background_layer, s_background_bitmap);
  
  // get the window and draw the new background
  Layer *window_layer = window_get_root_layer(s_main_window);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_background_layer));
  
  // re-add the datetime and weather layers
  layer_add_child(window_layer, text_layer_get_layer(s_timedate_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_weather_layer));
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
  
  // Get weather update every 30 minutes
  if(tick_time->tm_min % 30 == 0) {
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);

    // Add a key-value pair
    dict_write_uint8(iter, 0, 0);

    // Send the message!
    app_message_outbox_send();
  }
  
  // update the background every 2 minutes
  if(tick_time->tm_min % 2 == 0) {
    update_background();
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char temperature_buffer[8];
  static char conditions_buffer[32];
  static char weather_layer_buffer[32];
  
  // Read first item
  Tuple *t = dict_read_first(iterator);

  // For all items
  while(t != NULL) {
    // Which key was received?
    switch(t->key) {
    case KEY_TEMPERATURE:
      snprintf(temperature_buffer, sizeof(temperature_buffer), "%dF", (int)t->value->int32);
      break;
    case KEY_CONDITIONS:
      snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", t->value->cstring);
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_ERROR, "Key %d not recognized!", (int)t->key);
      break;
    }

    // Look for next item
    t = dict_read_next(iterator);
  }
  
  // Assemble full string and display
  snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s, %s", temperature_buffer, conditions_buffer);
  text_layer_set_text(s_weather_layer, weather_layer_buffer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
  
static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  // Open AppMessage
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}