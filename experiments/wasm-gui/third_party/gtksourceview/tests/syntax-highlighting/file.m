#import <stdio.h>
#import <Object.h>
@interface Lalala : Object
- (BOOL) sayHello;
@end

@implementation Lalala : Object
- (BOOL) sayHello
{
  printf ("Hello there!\n");
  return YES;
}
@end

int main (void)
{
  Lalala *obj = [[Lalala alloc] init];
  [obj sayHello];
  [obj free];
  return 0;
}
