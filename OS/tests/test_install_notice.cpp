#define FRESLEST_NOTICE_HOST_TEST 1
#include "../freslest/src/freslest/install_notice.cpp"
#include <cassert>
#include <cstdio>

static void reboot(){checked=visible=retryPending=false;shownAt=retryAt=0;message[0]=0;}
int main(){
  // Trial boots and fullscreen camera cannot consume the one-time notice.
  assert(!installNoticeUpdate(0,false,true)&&testOpens==0);
  assert(!installNoticeUpdate(10000,true,false)&&testOpens==0);
  assert(installNoticeUpdate(11000,true,true));
  assert(testPersistedVersion=="6.00"&&testCommits==1);
  assert(!strcmp(installNoticeText(),"Freslest OS 6.00 installed"));
  assert(!installNoticeUpdate(16999,true,true)&&installNoticeText());
  assert(installNoticeUpdate(17000,true,true)&&!installNoticeText());
  for(unsigned n=17001;n<20000;++n)assert(!installNoticeUpdate(n,true,true));
  assert(testCommits==1);
  // Reset while banner is visible or later: never show again for this version.
  reboot();assert(!installNoticeUpdate(1,true,true)&&!installNoticeText()&&testCommits==1);
  // A new version gets a new notice only after it has passed startup.
  strcpy(testApp.version,"6.01");reboot();
  assert(!installNoticeUpdate(10,false,true)&&testPersistedVersion=="6.00");
  assert(installNoticeUpdate(10001,true,true)&&testPersistedVersion=="6.01"&&testCommits==2);
  reboot();assert(!installNoticeUpdate(1,true,true)&&!installNoticeText());
  // No false 'once' promise if the durable write failed; retry is throttled.
  strcpy(testApp.version,"6.02");reboot();testNvsCommitFail=true;
  assert(!installNoticeUpdate(10,true,true)&&!installNoticeText()&&testPersistedVersion=="6.01");
  unsigned opens=testOpens;for(unsigned n=11;n<30010;++n)assert(!installNoticeUpdate(n,true,true));
  assert(testOpens==opens);testNvsCommitFail=false;
  assert(installNoticeUpdate(30010,true,true)&&testPersistedVersion=="6.02");
  reboot();assert(!installNoticeUpdate(0,true,true));
  // Clock wrap does not create a never-ending banner or another write.
  strcpy(testApp.version,"6.03");reboot();
  assert(installNoticeUpdate(UINT32_MAX-1000,true,true));
  assert(!installNoticeUpdate(4998,true,true)&&installNoticeText());
  assert(installNoticeUpdate(4999,true,true)&&!installNoticeText());
  assert(testOpens==testCloses);
  puts("PASS: install banner waits for healthy startup/visible header, persists before display, appears once per version across reset, expires after six seconds, throttles failed commits and handles clock wrap");
}
