/*
 *     Generated by class-dump 3.1.1.
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2006 by Steve Nygard.
 */

#import <BackRow/BRPanel.h>

@class BRImageControl, BRListControl, BRResumeMenuControlLayoutManager;

@interface BRResumeMenuControl : BRPanel
{
    BRListControl *_resumeMenu;
    BRImageControl *_blurControl;
    BRResumeMenuControlLayoutManager *_layoutManager;
}

- (id)init;
- (void)dealloc;
- (id)list;
- (void)setBlurredImage:(id)fp8;
- (void)setBlurOversizeFactor:(float)fp8;

@end

