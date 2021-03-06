/*
 *     Generated by class-dump 3.1.1.
 *
 *     class-dump is Copyright (C) 1997-1998, 2000-2001, 2004-2006 by Steve Nygard.
 */

#import "NSObject.h"

@class BRAccount, BRControllerStack, NSObject<BRTextEntryDelegate>;

@interface BRAccountSelector : NSObject
{
    BRAccount *_account;
    BRControllerStack *_stack;
    NSObject<BRTextEntryDelegate> *_accountNameCollector;
    NSObject<BRTextEntryDelegate> *_passwordCollector;
}

- (id)initWithParentController:(id)fp8;
- (void)dealloc;
- (void)selectAccountOfType:(id)fp8;
- (void)qualifyAccount:(id)fp8;

@end

