/* WinSCard.dll stand-in for building plugins.dat under wine only: wine
 * builds without winscard can't load libaribcam_plugin.dll, so the cache
 * would miss it. vlc-cache-gen only reads module descriptors and never calls
 * these. It is never shipped: package.mak puts it in the wine prefix. */
#define WINSCARDDATA /* defined here, not imported */
#include <windows.h>
#include <winscard.h>

static LONG unavailable(void)
{
    return SCARD_E_NO_SERVICE;
}

LONG WINAPI SCardEstablishContext(DWORD s, LPCVOID a, LPCVOID b, LPSCARDCONTEXT c)
{ (void)s; (void)a; (void)b; (void)c; return unavailable(); }
LONG WINAPI SCardReleaseContext(SCARDCONTEXT c)
{ (void)c; return unavailable(); }
LONG WINAPI SCardListReadersA(SCARDCONTEXT c, LPCSTR g, LPSTR r, LPDWORD n)
{ (void)c; (void)g; (void)r; (void)n; return unavailable(); }
LONG WINAPI SCardConnectA(SCARDCONTEXT c, LPCSTR r, DWORD s, DWORD p, LPSCARDHANDLE h, LPDWORD a)
{ (void)c; (void)r; (void)s; (void)p; (void)h; (void)a; return unavailable(); }
LONG WINAPI SCardDisconnect(SCARDHANDLE h, DWORD d)
{ (void)h; (void)d; return unavailable(); }
LONG WINAPI SCardTransmit(SCARDHANDLE h, LPCSCARD_IO_REQUEST s, LPCBYTE b, DWORD n,
                          LPSCARD_IO_REQUEST r, LPBYTE o, LPDWORD l)
{ (void)h; (void)s; (void)b; (void)n; (void)r; (void)o; (void)l; return unavailable(); }

const SCARD_IO_REQUEST g_rgSCardT1Pci = { SCARD_PROTOCOL_T1, sizeof(SCARD_IO_REQUEST) };
