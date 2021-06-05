// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#import "NonJailbrokenNoticeViewController.h"

#import "DOLJitManager.h"

@interface NonJailbrokenNoticeViewController ()

@end

@implementation NonJailbrokenNoticeViewController

- (void)viewDidLoad
{
  [super viewDidLoad];
}

- (void)viewWillAppear:(BOOL)animated
{
  if ([[DOLJitManager sharedManager] jitType] != DOLJitTypePTrace)
  {
    [self.m_quit_label setHidden:true];
    [self.m_bug_label setHidden:true];
    [self.m_ok_button setEnabled:true];
    [self.m_ok_button setAlpha:1.0f];
  }
}

- (void)viewDidAppear:(BOOL)animated
{
  [super viewDidAppear:animated];
  
  if ([[DOLJitManager sharedManager] jitType] == DOLJitTypePTrace)
  {
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
      [self.m_ok_button setEnabled:true];
      [self.m_ok_button setAlpha:1.0f];
    });
  }
}

- (IBAction)OKPressed:(id)sender
{
  [self.navigationController popViewControllerAnimated:true];
}

@end
