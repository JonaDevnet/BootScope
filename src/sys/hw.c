/*
 * BootScope - hardware: BIOS, memoria y discos (SMBIOS + SMART)
 * Copyright (c) 2026 Jonathan Rinaldi (JonaDevnet)
 * SPDX-License-Identifier: MIT
 */
#include "sys.h"
#include "util.h"
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef SMART_RCV_DRIVE_DATA
#define SMART_RCV_DRIVE_DATA CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif
#ifndef IOCTL_STORAGE_QUERY_PROPERTY
#define IOCTL_STORAGE_QUERY_PROPERTY CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

#define BS_PROP_SEEK_PENALTY 7
#define BS_PROP_ADAPTER_PROTO 49
#define BS_PROTO_NVME 3
#define BS_PROTO_NVME_LOG 2

typedef struct {
    DWORD ProtocolType;
    DWORD DataType;
    DWORD ProtocolDataRequestValue;
    DWORD ProtocolDataRequestSubValue;
    DWORD ProtocolDataOffset;
    DWORD ProtocolDataLength;
    DWORD FixedProtocolReturnData;
    DWORD Reserved[3];
} bs_proto_data;

typedef struct {
    DWORD Version;
    DWORD Size;
    bs_proto_data Data;
} bs_proto_desc;

/* ------------------------------------------------------------------ */
/* SMBIOS                                                              */
/* ------------------------------------------------------------------ */
static const char *smbios_str(const unsigned char *tabla, DWORD len, DWORD off,
                              unsigned char slen, int idx)
{
    DWORD p = off + slen;
    int i;
    if (idx <= 0) return "";
    for (i = 1; i < idx; i++) {
        while (p < len && tabla[p] != 0) p++;
        p++;
        if (p >= len) return "";
    }
    if (p >= len) return "";
    return (const char *)(tabla + p);
}

static int fecha_anio(const char *fecha, int *anio)
{
    int a = 0, n = 0;
    const char *p = fecha;
    if (!fecha || !*fecha) return 0;
    while (*p) {
        if (*p >= '0' && *p <= '9') {
            a = a * 10 + (*p - '0');
            n++;
        } else if (n == 4) {
            break;
        } else {
            a = 0;
            n = 0;
        }
        p++;
    }
    if (n != 4 || a < 1990 || a > 2100) return 0;
    *anio = a;
    return 1;
}

static void smbios_parse(bs_model *m)
{
    DWORD sz, len, off;
    unsigned char *buf;
    unsigned char *tabla;
    int bios_anio = 0;

    sz = GetSystemFirmwareTable('RSMB', 0, NULL, 0);
    if (sz < 16) return;
    buf = (unsigned char *)calloc(sz, 1);
    if (!buf) return;
    if (GetSystemFirmwareTable('RSMB', 0, buf, sz) != sz) {
        free(buf);
        return;
    }
    len = *(DWORD *)(buf + 4);
    if (len + 8 > sz) len = sz - 8;
    tabla = buf + 8;

    off = 0;
    while (off + 4 <= len) {
        unsigned char tipo = tabla[off];
        unsigned char slen = tabla[off + 1];
        DWORD p;

        if (slen < 4 || off + slen > len) break;

        if (tipo == 0) { /* BIOS */
            const char *ver = smbios_str(tabla, len, off, slen, tabla[off + 5]);
            const char *fec = smbios_str(tabla, len, off, slen, tabla[off + 8]);
            if (tabla[off + 5]) snprintf(m->bios.version, sizeof(m->bios.version), "%s", ver);
            if (tabla[off + 8] && fecha_anio(fec, &bios_anio))
                snprintf(m->bios.fecha, sizeof(m->bios.fecha), "%s", fec);
        } else if (tipo == 2) { /* placa base */
            const char *prod = smbios_str(tabla, len, off, slen, tabla[off + 5]);
            if (tabla[off + 5]) snprintf(m->bios.placa, sizeof(m->bios.placa), "%s", prod);
        } else if (tipo == 17) { /* modulo de memoria */
            if (m->n_dimms < BS_MAX_DIMMS) {
                bs_dimm *d = &m->dimms[m->n_dimms];
                DWORD mb = 0;
                WORD w = *(WORD *)(tabla + off + 0x0C);
                memset(d, 0, sizeof(*d));
                if (w == 0x7FFF && slen > 0x1F) {
                    DWORD ext = *(DWORD *)(tabla + off + 0x1C);
                    mb = ext & 0x7FFFFFFF;
                } else if (w == 0xFFFF) {
                    mb = 0;
                } else if (w & 0x8000) {
                    mb = (DWORD)(w & 0x7FFF) / 1024;
                } else {
                    mb = w;
                }
                d->gb = (int)(mb / 1024);
                if (mb == 0) {
                    /* slot vacio: no se muestra */
                } else {
                d->mhz = (slen > 0x16) ? *(WORD *)(tabla + off + 0x15) : 0;
                d->mhz_nominal = (slen > 0x21) ? *(WORD *)(tabla + off + 0x20) : 0;
                if (tabla[off + 0x10])
                    snprintf(d->slot, sizeof(d->slot), "%s",
                             smbios_str(tabla, len, off, slen, tabla[off + 0x10]));
                if (tabla[off + 0x1A])
                    snprintf(d->modulo, sizeof(d->modulo), "%s",
                             smbios_str(tabla, len, off, slen, tabla[off + 0x1A]));
                m->n_dimms++;
                }
            }
        }

        p = off + slen;
        if (p + 1 >= len) break;
        if (tabla[p] == 0 && tabla[p + 1] == 0) {
            p += 2;
        } else {
            while (p + 1 < len && !(tabla[p] == 0 && tabla[p + 1] == 0)) p++;
            p += 2;
        }
        if (p <= off) break;
        off = p;
    }
    free(buf);

    /* elementos visibles */
    if (m->bios.version[0]) {
        bs_item *it = bs_add_item(m);
        if (it) {
            int edad = 0;
            it->cat = BS_CAT_HARDWARE;
            snprintf(it->check_id, sizeof(it->check_id), "hw.bios");
            snprintf(it->titulo, sizeof(it->titulo), "BIOS %s", m->bios.version);
            snprintf(it->estado, sizeof(it->estado), "OK");
            snprintf(it->metrica, sizeof(it->metrica), "%s", m->bios.fecha[0] ? m->bios.fecha : "-");
            snprintf(it->detalle, sizeof(it->detalle), "Placa: %s", m->bios.placa);
            if (bios_anio) {
                SYSTEMTIME st;
                GetLocalTime(&st);
                edad = (int)st.wYear - bios_anio;
                if (edad < 0) edad = 0;
                snprintf(it->edad, sizeof(it->edad), "%d anos", edad);
            } else {
                snprintf(it->edad, sizeof(it->edad), "-");
            }
            if (edad >= 4) {
                it->riesgo = BS_RIESGO_MODERADO;
                snprintf(it->resultado, sizeof(it->resultado), "BIOS antiguo");
            } else {
                it->riesgo = BS_RIESGO_NULO;
            }
        }
    }

    if (m->n_dimms > 1) {
        int dispares = 0;
        for (int i = 1; i < m->n_dimms; i++)
            if (m->dimms[i].gb != m->dimms[0].gb) dispares = 1;
        for (int i = 0; i < m->n_dimms; i++) {
            bs_dimm *d = &m->dimms[i];
            bs_item *it = bs_add_item(m);
            if (!it) break;
            it->cat = BS_CAT_HARDWARE;
            snprintf(it->check_id, sizeof(it->check_id), "hw.ram");
            snprintf(it->titulo, sizeof(it->titulo), "RAM %s", d->slot[0] ? d->slot : "modulo");
            snprintf(it->estado, sizeof(it->estado), "OK");
            snprintf(it->metrica, sizeof(it->metrica), "%d GB", d->gb);
            snprintf(it->detalle, sizeof(it->detalle), "%s  %d MHz (nominal %d)",
                     d->modulo, d->mhz, d->mhz_nominal);
            snprintf(it->edad, sizeof(it->edad), "%d MHz", d->mhz);
            it->riesgo = dispares ? BS_RIESGO_MODERADO : BS_RIESGO_NULO;
            if (dispares) snprintf(it->resultado, sizeof(it->resultado), "modulos dispares");
        }
    }
}

/* ------------------------------------------------------------------ */
/* Discos y SMART                                                      */
/* ------------------------------------------------------------------ */
static int disco_del_sistema(void)
{
    HANDLE h;
    unsigned char buf[512];
    DWORD ret = 0;
    int disco = -1;

    h = CreateFileW(L"\\\\.\\C:", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                    OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
                        buf, sizeof(buf), &ret, NULL)) {
        VOLUME_DISK_EXTENTS *v = (VOLUME_DISK_EXTENTS *)buf;
        if (v->NumberOfDiskExtents > 0)
            disco = (int)v->Extents[0].DiskNumber;
    }
    CloseHandle(h);
    return disco;
}

static unsigned long long lee_entero(const unsigned char *p, int n)
{
    unsigned long long v = 0;
    int i;
    for (i = n - 1; i >= 0; i--)
        v = (v << 8) | p[i];
    return v;
}

static int nvme_salud(HANDLE h, char *salud, size_t sz, char *edad, size_t edsz,
                      int *flag)
{
    unsigned char in[8 + sizeof(bs_proto_data)];
    unsigned char out[4096];
    DWORD ret = 0;
    STORAGE_PROPERTY_QUERY *q;
    bs_proto_data *p;
    bs_proto_desc *d;
    const unsigned char *log;
    unsigned long long horas, ciclos, apagones;
    int temp, usado, spare;

    memset(&in, 0, sizeof(in));
    /* el bloque de protocolo va inmediatamente despues de PropertyId+QueryType (offset 8) */
    q = (STORAGE_PROPERTY_QUERY *)in;
    q->PropertyId = (STORAGE_PROPERTY_ID)BS_PROP_ADAPTER_PROTO;
    q->QueryType = PropertyStandardQuery;
    p = (bs_proto_data *)(in + 8);
    p->ProtocolType = BS_PROTO_NVME;
    p->DataType = BS_PROTO_NVME_LOG;
    p->ProtocolDataRequestValue = 0x02;
    p->ProtocolDataOffset = (DWORD)sizeof(bs_proto_data);
    p->ProtocolDataLength = 512;

    if (!DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, in, sizeof(in),
                         out, sizeof(out), &ret, NULL))
        return 0;
    d = (bs_proto_desc *)out;
    log = (const unsigned char *)&d->Data + d->Data.ProtocolDataOffset;

    temp  = (int)(lee_entero(log + 1, 2)) - 273;
    spare = (int)log[3];
    usado = (int)log[5];
    ciclos = lee_entero(log + 112, 8);
    horas  = lee_entero(log + 128, 8);
    apagones = lee_entero(log + 144, 8);

    snprintf(salud, sz, "%d C, %d%% usado, spare %d%%, %llu apagones inseguros",
             temp, usado, spare, apagones);
    if (horas == 0) snprintf(edad, edsz, "%llu ciclos", ciclos);
    else snprintf(edad, edsz, "%llu h (%.1f anos), %llu ciclos",
                  horas, (double)horas / 8760.0, ciclos);
    if (usado >= 80 || log[0] != 0) *flag = 2;
    else if (usado >= 50) *flag = 1;
    return 1;
}

static int ata_salud(HANDLE h, char *salud, size_t sz, char *edad, size_t edsz,
                     int *flag)
{
    unsigned char inbuf[sizeof(SENDCMDINPARAMS) + 512];
    unsigned char outbuf[sizeof(SENDCMDOUTPARAMS) + 512];
    SENDCMDINPARAMS *in = (SENDCMDINPARAMS *)inbuf;
    DWORD ret = 0;
    const unsigned char *attr;
    unsigned long long horas = 0;
    int ciclos = 0, temp = -1, reasig = 0, pend = 0;
    int i;

    memset(inbuf, 0, sizeof(inbuf));
    memset(outbuf, 0, sizeof(outbuf));
    in->cBufferSize = 512;
    in->irDriveRegs.bCommandReg = 0xD0;
    in->irDriveRegs.bFeaturesReg = 0xD0;
    in->irDriveRegs.bCylLowReg = 0x4F;
    in->irDriveRegs.bCylHighReg = 0xC2;
    in->irDriveRegs.bDriveHeadReg = 0xA0;
    in->bDriveNumber = 0;

    if (!DeviceIoControl(h, SMART_RCV_DRIVE_DATA, inbuf, sizeof(SENDCMDINPARAMS) + 512,
                         outbuf, sizeof(outbuf), &ret, NULL))
        return 0;

    attr = outbuf + offsetof(SENDCMDOUTPARAMS, bBuffer) + 2;
    for (i = 0; i < 30; i++) {
        const unsigned char *a = attr + i * 12;
        int id = a[0];
        const unsigned char *raw = a + 5;
        if (id == 0) continue;
        if (id == 9)   horas = lee_entero(raw, 4);
        if (id == 12)  ciclos = (int)lee_entero(raw, 4);
        if (id == 194) temp = (int)raw[0];
        if (id == 5)   reasig = (int)raw[0];
        if (id == 197) pend = (int)raw[0];
    }

    snprintf(salud, sz, "%d C, reasignados %d, pendientes %d", temp, reasig, pend);
    if (horas)
        snprintf(edad, edsz, "%llu h (%.1f anos), %d ciclos",
                 horas, (double)horas / 8760.0, ciclos);
    else
        snprintf(edad, edsz, "%d ciclos", ciclos);
    if (reasig > 0 || pend > 0) *flag = 2;
    else if (horas > 40000) *flag = 1;
    return 1;
}

static int ata_identify_tipo(HANDLE h, char *tipo, size_t tsz)
{
    unsigned char inbuf[sizeof(SENDCMDINPARAMS) + 512];
    unsigned char outbuf[sizeof(SENDCMDOUTPARAMS) + 512];
    SENDCMDINPARAMS *in = (SENDCMDINPARAMS *)inbuf;
    DWORD ret = 0;
    const unsigned char *id;
    unsigned short rot;

    memset(inbuf, 0, sizeof(inbuf));
    memset(outbuf, 0, sizeof(outbuf));
    in->cBufferSize = 512;
    in->irDriveRegs.bCommandReg = 0xEC;   /* IDENTIFY DEVICE */
    in->irDriveRegs.bDriveHeadReg = 0xA0;
    in->bDriveNumber = 0;

    if (!DeviceIoControl(h, SMART_RCV_DRIVE_DATA, inbuf, sizeof(inbuf),
                         outbuf, sizeof(outbuf), &ret, NULL))
        return 0;

    id = outbuf + offsetof(SENDCMDOUTPARAMS, bBuffer);
    rot = (unsigned short)(id[434] | (id[435] << 8)); /* word 217 */
    if (rot == 1) snprintf(tipo, tsz, "SSD");
    else if (rot == 0) snprintf(tipo, tsz, "?");
    else snprintf(tipo, tsz, "HDD");
    return 1;
}

static void discos_collect(bs_model *m)
{
    int sistema = disco_del_sistema();
    int i;

    for (i = 0; i < BS_MAX_DISKS; i++) {
        wchar_t ruta[64];
        HANDLE h;
        GET_LENGTH_INFORMATION li;
        DWORD ret = 0;
        unsigned char buf[1024];
        STORAGE_DEVICE_DESCRIPTOR *sd;
        STORAGE_PROPERTY_QUERY q;
        bs_disk *d;
        char modelo[128] = "";
        int flag = 0, ok_salud = 0;
        const char *bus = "otro";

        swprintf(ruta, 64, L"\\\\.\\PhysicalDrive%d", i);
        h = CreateFileW(ruta, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE)
            h = CreateFileW(ruta, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) continue;

        if (m->n_discos >= BS_MAX_DISKS) { CloseHandle(h); break; }
        d = &m->discos[m->n_discos];
        memset(d, 0, sizeof(*d));

        if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &li,
                            sizeof(li), &ret, NULL))
            d->bytes = (unsigned long long)li.Length.QuadPart;

        memset(&q, 0, sizeof(q));
        memset(buf, 0, sizeof(buf));
        q.PropertyId = StorageDeviceProperty;
        q.QueryType = PropertyStandardQuery;
        if (DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q),
                            buf, sizeof(buf), &ret, NULL)) {
            sd = (STORAGE_DEVICE_DESCRIPTOR *)buf;
            if (sd->ProductIdOffset && sd->ProductIdOffset < sizeof(buf)) {
                snprintf(modelo, sizeof(modelo), "%s", (char *)(buf + sd->ProductIdOffset));
                bs_trim(modelo);
            }
            switch (sd->BusType) {
            case BusTypeNvme: bus = "NVMe"; break;
            case BusTypeSata: bus = "SATA"; break;
            case BusTypeAta:  bus = "ATA"; break;
            case BusTypeUsb:  bus = "USB"; break;
            default: break;
            }
        }

        if (strcmp(bus, "NVMe") == 0)
            snprintf(d->tipo, sizeof(d->tipo), "SSD");
        else if (!ata_identify_tipo(h, d->tipo, sizeof(d->tipo)))
            snprintf(d->tipo, sizeof(d->tipo), "?");

        if (strcmp(bus, "NVMe") == 0)
            ok_salud = nvme_salud(h, d->salud, sizeof(d->salud), d->edad, sizeof(d->edad), &flag);
        else if (strcmp(bus, "SATA") == 0 || strcmp(bus, "ATA") == 0)
            ok_salud = ata_salud(h, d->salud, sizeof(d->salud), d->edad, sizeof(d->edad), &flag);

        if (!ok_salud) {
            snprintf(d->salud, sizeof(d->salud), "%s",
                     bs_is_admin() ? "SMART no disponible" : "requiere administrador");
            snprintf(d->edad, sizeof(d->edad), "-");
            flag = 0;
        }

        snprintf(d->modelo, sizeof(d->modelo), "%s", modelo[0] ? modelo : "disco");
        snprintf(d->bus, sizeof(d->bus), "%s", bus);
        if (!ok_salud)
            snprintf(d->estado, sizeof(d->estado), "Sin datos");
        else
            snprintf(d->estado, sizeof(d->estado), "%s",
                     flag == 2 ? "Revisar" : (flag == 1 ? "Atencion" : "Healthy"));
        d->sistema = (i == sistema) ? 1 : 0;
        m->n_discos++;

        {
            bs_item *it = bs_add_item(m);
            if (it) {
                it->cat = BS_CAT_HARDWARE;
                snprintf(it->check_id, sizeof(it->check_id), "hw.disco");
                snprintf(it->titulo, sizeof(it->titulo), "%s%s", d->modelo,
                         d->sistema ? " (sistema)" : "");
                snprintf(it->estado, sizeof(it->estado), "%s", d->estado);
                if (d->bytes > 0)
                    snprintf(it->metrica, sizeof(it->metrica), "%llu GB",
                             d->bytes / (1024ULL * 1024ULL * 1024ULL));
                else
                    snprintf(it->metrica, sizeof(it->metrica), "--");
                snprintf(it->edad, sizeof(it->edad), "%s", d->edad);
                snprintf(it->detalle, sizeof(it->detalle), "%s %s - %s", d->bus, d->tipo, d->salud);
                it->riesgo = (flag == 2) ? BS_RIESGO_ALTO :
                             (flag == 1 ? BS_RIESGO_MODERADO : BS_RIESGO_NULO);
                if (flag == 2) snprintf(it->resultado, sizeof(it->resultado), "salud comprometida");
                else if (flag == 1) snprintf(it->resultado, sizeof(it->resultado), "vigilar");
            }
        }
        CloseHandle(h);
    }
}

void hw_collect(bs_model *m)
{
    smbios_parse(m);
    discos_collect(m);
}

