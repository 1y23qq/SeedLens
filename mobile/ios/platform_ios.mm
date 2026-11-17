#import <UIKit/UIKit.h>
#include <SDL3/SDL.h>
#include "../../src/mobile_platform.h"

namespace seedlens {void mobile_set_notice(std::string);}
@interface SeedLensExportDelegate:NSObject<UIDocumentPickerDelegate>
@end
@implementation SeedLensExportDelegate
-(void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls{
    (void)controller;(void)urls;seedlens::mobile_set_notice("保存しました / Saved");
}
-(void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller{
    (void)controller;seedlens::mobile_set_notice("保存をキャンセルしました / Cancelled");
}
@end
namespace seedlens {
static SDL_Window* app_window=nullptr;
void mobile_initialize(SDL_Window* window){
    app_window=window;
    [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillChangeFrameNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification* note){
        UIWindow* native=(__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(app_window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr);
        CGRect rect=[native convertRect:[note.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue] fromWindow:nil];
        CGRect overlap=CGRectIntersection(native.bounds,rect);
        bool docked=!CGRectIsNull(overlap)&&CGRectGetMaxY(overlap)>=native.bounds.size.height-1&&overlap.size.width>native.bounds.size.width/2;
        mobile_set_keyboard_height(docked?int(overlap.size.height):0);
    }];
    [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardWillHideNotification object:nil queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification*){
        mobile_set_keyboard_height(0);
    }];
}
void mobile_export_file(const std::filesystem::path& path,const char* mime){
    (void)mime;
    NSString* filename=[NSString stringWithUTF8String:path.string().c_str()];
    dispatch_async(dispatch_get_main_queue(),^{
        UIWindow* window=(__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(app_window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr);
        UIViewController* presenter=window.rootViewController;
        while(presenter.presentedViewController)presenter=presenter.presentedViewController;
        UIDocumentPickerViewController* picker=[[UIDocumentPickerViewController alloc] initForExportingURLs:@[[NSURL fileURLWithPath:filename]] asCopy:YES];
        static SeedLensExportDelegate* delegate=[[SeedLensExportDelegate alloc] init];
        picker.delegate=delegate;[presenter presentViewController:picker animated:YES completion:nil];
    });
}
}
