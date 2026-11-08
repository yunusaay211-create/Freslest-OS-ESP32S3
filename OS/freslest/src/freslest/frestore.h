#pragma once

struct FreStoreTrust { char origin[145], key[145]; };
bool frestoreReadTrust(FreStoreTrust &out);
bool frestoreBusy();
void frestoreEnter(bool installedOnly);
void frestoreExit();
void frestoreLoop();
