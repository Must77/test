# ESP32-S3 LCD BOOT Button Image Display Guide

## Overview
This guide explains how the BOOT button image display functionality works in the factory demo and how it was modified.

## Implementation Details

### 1. Button Event System

**File**: `components/button_bsp/button_bsp.c`

The BOOT button (GPIO 0) uses the multi-button library for event detection:
- **Single-click**: Event bit 0
- **Double-click**: Event bit 1  
- **Long-press**: Event bit 5

Events are posted to a FreeRTOS event group (`key_groups`) for task synchronization.

### 2. Event Handler

**File**: `components/user_app/user_app.cpp`  
**Function**: `example_button_task()`

This FreeRTOS task continuously monitors button events:

```cpp
void example_button_task(void *arg)
{
    // Wait for button events
    EventBits_t even = xEventGroupWaitBits(key_groups, even_set_bit, pdTRUE, pdFALSE, pdMS_TO_TICKS(2500));
    
    if(READ_BIT(even, 0))    // Single-click
    {
        display_image_from_sdcard(SDCARD_IMAGE_PATH);
    }
    else if(READ_BIT(even, 1))  // Double-click
    {
        // Toggle backlight brightness
    }
    else if(READ_BIT(even, 5))  // Long-press
    {
        // Test SD card write/read
    }
}
```

### 3. Image Display Function

**File**: `components/user_app/user_app.cpp`  
**Function**: `display_image_from_sdcard()`

This function handles the complete image display workflow:

1. **File Validation**: Checks if the image file exists using `stat()`
2. **Container Creation**: Creates an LVGL container object if not already present
3. **Image Loading**: Creates an LVGL image object and sets the source
4. **Error Handling**: Shows error message if file is missing

```cpp
void display_image_from_sdcard(const char *path)
{
    // Check if file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        // Show error message
        return;
    }
    
    // Create image container
    if (img_container == NULL) {
        img_container = lv_obj_create(lv_scr_act());
        // Configure container...
    }
    
    // Create and display image
    lv_obj_t *img = lv_img_create(img_container);
    char lvgl_path[LVGL_PATH_MAX];
    snprintf(lvgl_path, sizeof(lvgl_path), "S:%s", path);
    lv_img_set_src(img, lvgl_path);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}
```

### 4. LVGL Filesystem Driver

**File**: `components/user_app/user_app.cpp`  
**Function**: `lv_fs_posix_init()`

The filesystem driver enables LVGL to access SD card files:

- **Drive Letter**: 'S' (for SD card)
- **Path Format**: `S:/sdcard/filename.jpg`
- **Backend**: POSIX file operations (open, close, read, seek, tell)

```cpp
static void lv_fs_posix_init(void)
{
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    
    fs_drv.letter = 'S';  // Drive letter for SD card
    fs_drv.open_cb = fs_open_cb;
    fs_drv.close_cb = fs_close_cb;
    fs_drv.read_cb = fs_read_cb;
    fs_drv.seek_cb = fs_seek_cb;
    fs_drv.tell_cb = fs_tell_cb;
    
    lv_fs_drv_register(&fs_drv);
}
```

### 5. JPEG Support

**Configuration**: `sdkconfig`

The SJPG (Split JPEG) decoder is enabled for JPEG image support:
```
CONFIG_LV_USE_SJPG=y
```

This allows LVGL to decode and display JPEG images from the SD card.

## Changes Made

### Task ① - Replace Image

**Modified**: `components/user_app/user_app.cpp` (line 26)

Changed the image path constant:
```cpp
// Before:
#define SDCARD_IMAGE_PATH "/sdcard/1.jpg"

// After:
#define SDCARD_IMAGE_PATH "/sdcard/2.jpg"
```

### Task ② - SD Card Reading

**Status**: ✅ Already Implemented

The SD card reading functionality is fully implemented using:
- LVGL POSIX filesystem driver
- SJPG decoder for JPEG images
- Error handling and status reporting

## How to Use

### Preparing the SD Card

1. Format an SD card as FAT32
2. Place a JPEG image named `2.jpg` in the root directory
3. Insert the SD card into the ESP32-S3 board

### Testing the Functionality

1. **Flash the Program**:
   ```bash
   cd 09_FactoryProgram
   idf.py build flash monitor
   ```

2. **Press BOOT Button (Single-Click)**:
   - The image from `/sdcard/2.jpg` will be displayed full-screen
   - If the file is missing, an error message will be shown

3. **Other Button Actions**:
   - **Double-click**: Toggles LCD backlight (ON/OFF)
   - **Long-press**: Tests SD card write/read functionality

### Expected Behavior

**Success Case**:
- Image displays full-screen
- Console log: "Image display created successfully"

**Error Case** (file not found):
- Red error message on screen: "Error: Image not found"
- Console log: "Image file not found: /sdcard/2.jpg"

## Customization

### Changing the Image Path

Edit line 26 in `components/user_app/user_app.cpp`:
```cpp
#define SDCARD_IMAGE_PATH "/sdcard/your_image.jpg"
```

### Changing Button Actions

Edit the `example_button_task()` function in `components/user_app/user_app.cpp`:
```cpp
if(READ_BIT(even, 0))    // Single-click
{
    // Your custom action here
}
```

### Supporting Different Image Formats

The current implementation supports JPEG images via SJPG decoder. To add other formats:

1. Enable the appropriate decoder in `sdkconfig`:
   - PNG: `CONFIG_LV_USE_PNG=y`
   - BMP: `CONFIG_LV_USE_BMP=y`
   - GIF: `CONFIG_LV_USE_GIF=y`

2. The `lv_img_set_src()` function automatically detects the format

## Troubleshooting

### Image Not Displaying

1. **Check SD Card**: Ensure the SD card is properly inserted
2. **Verify File**: Confirm `2.jpg` exists in `/sdcard/` directory
3. **Check Format**: Image must be JPEG format
4. **Monitor Logs**: Check serial output for error messages

### SD Card Not Detected

1. Check SD card formatting (FAT32)
2. Verify SD card connections
3. Look for "sdcard : X.XXG" on the display (main screen)

### Memory Issues

If the device crashes when loading large images:
1. Reduce image resolution (recommended: 320x820 or smaller)
2. Compress JPEG to reduce file size
3. Check available heap memory in logs

## Technical Notes

### Memory Management

- Image containers are reused to prevent memory leaks
- LVGL manages image buffer allocation automatically
- Failed loads clean up resources properly

### Thread Safety

- All LVGL operations run in the LVGL task context
- Button events use FreeRTOS event groups for synchronization
- No direct GPIO interrupts in LVGL operations

### Performance

- SJPG decoder is optimized for embedded systems
- Image loading time depends on:
  - SD card speed
  - Image file size
  - Image resolution

## Related Files

- `components/user_app/user_app.cpp` - Main application logic
- `components/user_app/user_app.h` - Header file
- `components/button_bsp/button_bsp.c` - Button driver
- `components/sdcard_bsp/` - SD card driver
- `sdkconfig` - LVGL and decoder configuration

## References

- [LVGL Documentation](https://docs.lvgl.io/)
- [LVGL Filesystem](https://docs.lvgl.io/master/overview/file-system.html)
- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/)
