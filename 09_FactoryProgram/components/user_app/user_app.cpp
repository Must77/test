#include <stdio.h>
#include "user_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "button_bsp.h"
#include "sdcard_bsp.h"
#include "lcd_bl_pwm_bsp.h"
#include "gui_guider.h"
#include "lvgl.h"
#include "esp_log.h"
#include "adc_bsp.h"
#include "i2c_bsp.h"
#include "i2c_equipment.h"
#include "ble_scan_bsp.h"
#include "esp_wifi_bsp.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

lv_ui user_ui;

static const char *TAG_IMG = "ImageDisplay";
static lv_obj_t *img_container = NULL;

// LVGL POSIX filesystem driver functions
static void * fs_open_cb(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
{
    int flags = 0;
    if(mode == LV_FS_MODE_WR) flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if(mode == LV_FS_MODE_RD) flags = O_RDONLY;
    else if(mode == LV_FS_MODE_WR | LV_FS_MODE_RD) flags = O_RDWR | O_CREAT;
    
    int *fd = (int *)lv_mem_alloc(sizeof(int));
    if(fd == NULL) return NULL;
    
    *fd = open(path, flags, 0666);
    if(*fd < 0) {
        lv_mem_free(fd);
        return NULL;
    }
    return (void *)fd;
}

static lv_fs_res_t fs_close_cb(lv_fs_drv_t * drv, void * file_p)
{
    int *fd = (int *)file_p;
    close(*fd);
    lv_mem_free(fd);
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_read_cb(lv_fs_drv_t * drv, void * file_p, void * buf, uint32_t btr, uint32_t * br)
{
    int *fd = (int *)file_p;
    int result = read(*fd, buf, btr);
    if(result < 0) return LV_FS_RES_UNKNOWN;
    *br = result;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_seek_cb(lv_fs_drv_t * drv, void * file_p, uint32_t pos, lv_fs_whence_t whence)
{
    int *fd = (int *)file_p;
    int w;
    if(whence == LV_FS_SEEK_SET) w = SEEK_SET;
    else if(whence == LV_FS_SEEK_CUR) w = SEEK_CUR;
    else if(whence == LV_FS_SEEK_END) w = SEEK_END;
    else return LV_FS_RES_UNKNOWN;
    
    if(lseek(*fd, pos, w) < 0) return LV_FS_RES_UNKNOWN;
    return LV_FS_RES_OK;
}

static lv_fs_res_t fs_tell_cb(lv_fs_drv_t * drv, void * file_p, uint32_t * pos_p)
{
    int *fd = (int *)file_p;
    *pos_p = lseek(*fd, 0, SEEK_CUR);
    if(*pos_p < 0) return LV_FS_RES_UNKNOWN;
    return LV_FS_RES_OK;
}

// Initialize LVGL filesystem driver for POSIX
static void lv_fs_posix_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    
    fs_drv.letter = 'S';  // Drive letter for SD card
    fs_drv.cache_size = 0;
    
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;
    
    lv_fs_drv_register(&fs_drv);
    ESP_LOGI(TAG_IMG, "LVGL POSIX filesystem driver registered with letter 'S'");
}

// Function to display image from SD card
void display_image_from_sdcard(const char *path)
{
    ESP_LOGI(TAG_IMG, "Attempting to display image from: %s", path);
    
    // Check if file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG_IMG, "Image file not found: %s", path);
        return;
    }
    
    // Create or reuse image container
    if (img_container == NULL) {
        img_container = lv_obj_create(lv_scr_act());
        lv_obj_set_size(img_container, LV_HOR_RES, LV_VER_RES);
        lv_obj_align(img_container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(img_container, lv_color_black(), 0);
        lv_obj_set_style_border_width(img_container, 0, 0);
        lv_obj_set_style_pad_all(img_container, 0, 0);
    } else {
        lv_obj_clean(img_container);
    }
    
    // Create image object
    lv_obj_t *img = lv_img_create(img_container);
    
    // Set image source using LVGL filesystem path
    // Format: "S:/sdcard/1.jpg" where S is the drive letter
    char lvgl_path[64];
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path);
    
    lv_img_set_src(img, lvgl_path);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    
    // Make container visible and bring to front
    lv_obj_clear_flag(img_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(img_container);
    
    ESP_LOGI(TAG_IMG, "Image display created successfully");
}

void user_color_task(void *arg);
void example_user_task(void *arg);
void example_sdcard_task(void *arg);
void example_button_task(void *arg);
void example_scan_wifi_ble_task(void *arg);
void User_LCD_Before_Init(void)
{
  lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);
  i2c_master_Init();
}
void User_LCD_After_Init(void)
{
  setup_ui(&user_ui);
  user_button_init();
  _sdcard_init();
  lv_fs_posix_init();  // Initialize LVGL filesystem driver
  adc_bsp_init();
  i2c_rtc_setup();
  i2c_rtc_setTime(2025,6,20,19,1,30);
  i2c_qmi_setup();
  espwifi_init();
  xTaskCreatePinnedToCore(user_color_task, "user_color_task", 3 * 1024, &user_ui , 2, NULL,0); //color
  xTaskCreatePinnedToCore(example_sdcard_task, "example_sdcard_task", 3 * 1024, &user_ui, 2, NULL,0);  // sd
  xTaskCreatePinnedToCore(example_user_task, "example_user_task", 3 * 1024, &user_ui, 2, NULL,0);   // user
  xTaskCreatePinnedToCore(example_button_task, "example_button_task", 3000, (void *)&user_ui, 2, NULL,0);   
  xTaskCreatePinnedToCore(example_scan_wifi_ble_task, "example_scan_wifi_ble_task", 3000, (void *)&user_ui, 2, NULL,0);   
}
void example_scan_wifi_ble_task(void *arg)
{
  lv_ui *Send_ui = (lv_ui *)arg;
  char send_lvgl[50] = {""};
  uint8_t ble_scan_count = 0;
  uint8_t ble_mac[6];
  EventBits_t even = xEventGroupWaitBits(wifi_even_,0x02,pdTRUE,pdTRUE,pdMS_TO_TICKS(30000)); 
  espwifi_deinit(); //释放WIFI
  ble_scan_prepare();
  ble_stack_init();
  ble_scan_start();
  for(;xQueueReceive(ble_queue,ble_mac,3500) == pdTRUE;)
  {
    //ESP_LOGI(TAG, "%d",connt);
    ble_scan_count++;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if(READ_BIT(even,1))
  {
    snprintf(send_lvgl,45,"ble : %d wifi : %d",ble_scan_count,user_esp_bsp.apNum);
  }
  else
  {
    snprintf(send_lvgl,45,"ble : %d wifi : P",ble_scan_count);
  }
  lv_label_set_text(Send_ui->screen_label_8, send_lvgl);
  ble_stack_deinit();//释放BLE
  vTaskDelete(NULL);
}
void example_button_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  uint8_t ui_over = 2;
  uint8_t bl_test = 255;
  uint32_t sdcard_test = 0;
  char sdcard_send_buf[50] = {""};
  char sdcard_read_buf[50] = {""};
  uint8_t even_set_bit = 0;
  SET_BIT(even_set_bit,0);
  SET_BIT(even_set_bit,1);
  SET_BIT(even_set_bit,5);
  for(;;)
  {
    EventBits_t even = xEventGroupWaitBits(key_groups,even_set_bit,pdTRUE,pdFALSE,pdMS_TO_TICKS(2500));
    if(READ_BIT(even,0))    //单击 - Display image from SD card
    {
      ESP_LOGI(TAG_IMG, "Button single-click detected, displaying image");
      display_image_from_sdcard("/sdcard/1.jpg");
    }
    else if(READ_BIT(even,1))  //双击
    {
      switch (bl_test)
      {
        case 255:
          bl_test = 0;
          setUpduty(LCD_PWM_MODE_0);
          break;
        case 0:
          bl_test = 255;
          setUpduty(LCD_PWM_MODE_255);
          break;
        default:
          break;
      }
    }
    else if(READ_BIT(even,5))  //长按
    {
      sdcard_test++;
      snprintf(sdcard_send_buf,50,"China is the greatest country : %ld",sdcard_test);
      sdcard_file_write("/sdcard/Test.txt",sdcard_send_buf);
      sdcard_file_read("/sdcard/Test.txt",sdcard_read_buf,NULL);
      if(!strcmp(sdcard_send_buf,sdcard_read_buf))
      {
        ESP_LOGI("sdcardTest", "sd card Test pass");
        lv_label_set_text(ui->screen_label_6, "sd Test Pass");
      }
      else
      {
        lv_label_set_text(ui->screen_label_6, "sd Test Fail");
      }
    }
    else
    {
      lv_label_set_text(ui->screen_label_6, "");
    }
  }
}
void example_sdcard_task(void *arg)
{
  lv_ui *Send_ui = (lv_ui *)arg;
  char send_lvgl[50] = {""};
  EventBits_t even = xEventGroupWaitBits(sdcard_even_,0x01,pdTRUE,pdTRUE,pdMS_TO_TICKS(15000)); //等待sdcard 成功
  if( READ_BIT(even,0) )
  {
    snprintf(send_lvgl,45,"sdcard : %.2fG",user_sdcard_bsp.sdcard_size);
    lv_label_set_text(Send_ui->screen_label_3,send_lvgl);
  }
  else
  {
    lv_label_set_text(Send_ui->screen_label_3,"null");
  }
  vTaskDelete(NULL);
}

void user_color_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  lv_obj_clear_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //不可移动
  lv_obj_clear_flag(ui->screen_img_1,LV_OBJ_FLAG_HIDDEN);  //显示
  lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_clear_flag(ui->screen_img_2,LV_OBJ_FLAG_HIDDEN); //显示
  lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_3, LV_OBJ_FLAG_HIDDEN);
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_clear_flag(ui->screen_img_3,LV_OBJ_FLAG_HIDDEN); //显示
  lv_obj_add_flag(ui->screen_img_2, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui->screen_img_1, LV_OBJ_FLAG_HIDDEN);
  vTaskDelay(pdMS_TO_TICKS(1500));
  lv_obj_add_flag(ui->screen_carousel_1,LV_OBJ_FLAG_SCROLLABLE); //可移动
  lv_obj_scroll_by(ui->screen_carousel_1,-320,0,LV_ANIM_ON);//向左滑动320
  vTaskDelete(NULL); //删除任务
}


void example_user_task(void *arg)
{
  lv_ui *ui = (lv_ui *)arg;
  uint32_t stimes = 0;
  uint32_t rtc_time = 0;
  uint32_t qmi_time = 0;
  uint32_t adc_time = 0;
  char rtc_send_buf[50] = {0};
  char imu_send_buf[50] = {0};
  uint8_t imu_flag = 0;
  char adc_send_buf[30] = {0};
  float adc_value = 0;
  for(;;)
  {
    if(stimes - rtc_time > 4) //1s
    {
      rtc_time = stimes;
      RtcDateTime_t rtc_data = i2c_rtc_get();
      snprintf(rtc_send_buf,45,"rtc : \n%d/%d/%d\n%02d:%02d:%02d",rtc_data.year,rtc_data.month,rtc_data.day,rtc_data.hour,rtc_data.minute,rtc_data.second);
      lv_label_set_text(ui->screen_label_4, rtc_send_buf);
    }
    if(stimes - qmi_time > 4) //1s
    {
      qmi_time = stimes;
      ImuDate_t imu_data = i2c_imu_get();
      if(imu_flag == 0)
      {snprintf(imu_send_buf,50,"acc : \n%.2fg \n%.2fg \n%.2fg",imu_data.accx,imu_data.accy,imu_data.accz);imu_flag = 1;}
      else
      {snprintf(imu_send_buf,50,"gyro : \n%.2fdps \n%.2fdps \n%.2fdps",imu_data.gyrox,imu_data.gyroy,imu_data.gyroz);imu_flag = 0;}
      lv_label_set_text(ui->screen_label_5, imu_send_buf);
    }
    if(stimes - adc_time > 9) //2s
    {
      adc_time = stimes;
      adc_get_value(&adc_value,NULL);
      if(adc_value)
      {
        snprintf(adc_send_buf,30,"vbat : %.2fV",adc_value);
        lv_label_set_text(ui->screen_label_7, adc_send_buf);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
    stimes++;
  }
}