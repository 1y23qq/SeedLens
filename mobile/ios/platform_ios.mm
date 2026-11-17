#import <UIKit/UIKit.h>
#include <SDL3/SDL.h>
#include "../../src/mobile_platform.h"

namespace seedlens {void mobile_set_notice(std::string);}
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
        NSFileManager* manager=[NSFileManager defaultManager];
        NSURL* documents=[manager URLsForDirectory:NSDocumentDirectory inDomains:NSUserDomainMask].firstObject;
        NSURL* directory=[documents URLByAppendingPathComponent:@"Exports" isDirectory:YES];
        NSURL* source=[NSURL fileURLWithPath:filename];
        NSURL* destination=[directory URLByAppendingPathComponent:source.lastPathComponent];
        NSError* error=nil;
        if(![manager createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error]){
            mobile_set_notice("保存できませんでした / Could not save");return;
        }
        for(int i=2;[manager fileExistsAtPath:destination.path];++i){
            NSString* name=[NSString stringWithFormat:@"%@ (%d).%@",source.lastPathComponent.stringByDeletingPathExtension,i,source.pathExtension];
            destination=[directory URLByAppendingPathComponent:name];
        }
        if(![manager copyItemAtURL:source toURL:destination error:&error]){
            mobile_set_notice("保存できませんでした / Could not save");return;
        }
        UIWindow* window=(__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(app_window),SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER,nullptr);
        UIViewController* presenter=window.rootViewController;
        while(presenter.presentedViewController)presenter=presenter.presentedViewController;
        UIActivityViewController* share=[[UIActivityViewController alloc] initWithActivityItems:@[destination] applicationActivities:nil];
        if(UIDevice.currentDevice.userInterfaceIdiom==UIUserInterfaceIdiomPad)share.modalPresentationStyle=UIModalPresentationPopover;
        share.excludedActivityTypes=@[UIActivityTypeSaveToCameraRoll];
        share.completionWithItemsHandler=^(UIActivityType type,BOOL complete,NSArray* items,NSError* activity_error){
            (void)type;(void)complete;(void)items;
            mobile_set_notice(activity_error?"共有できませんでした / Could not share":"保存先: ファイル → SeedLens → Exports / Saved in Files → SeedLens → Exports");
        };
        if(share.popoverPresentationController){
            share.popoverPresentationController.sourceView=presenter.view;
            share.popoverPresentationController.sourceRect=CGRectMake(CGRectGetMidX(presenter.view.bounds),CGRectGetMidY(presenter.view.bounds),1,1);
            share.popoverPresentationController.permittedArrowDirections=0;
        }
        [presenter presentViewController:share animated:YES completion:nil];
    });
}
}
