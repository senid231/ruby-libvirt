/*
 * domain.c: virDomain methods
 *
 * Copyright (C) 2007,2010 Red Hat Inc.
 * Copyright (C) 2013 Chris Lalancette <clalancette@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <stdint.h>
#include <ruby.h>
#include <ruby/st.h>
#include <libvirt/libvirt.h>
#if HAVE_VIRDOMAINQEMUMONITORCOMMAND
#include <libvirt/libvirt-qemu.h>
#endif
#include <libvirt/virterror.h>
#include "common.h"
#include "connect.h"
#include "extconf.h"
#include "stream.h"

#ifndef HAVE_TYPE_VIRTYPEDPARAMETERPTR
#define VIR_TYPED_PARAM_INT VIR_DOMAIN_SCHED_FIELD_INT
#define VIR_TYPED_PARAM_UINT VIR_DOMAIN_SCHED_FIELD_UINT
#define VIR_TYPED_PARAM_LLONG VIR_DOMAIN_SCHED_FIELD_LLONG
#define VIR_TYPED_PARAM_ULLONG VIR_DOMAIN_SCHED_FIELD_ULLONG
#define VIR_TYPED_PARAM_DOUBLE VIR_DOMAIN_SCHED_FIELD_DOUBLE
#define VIR_TYPED_PARAM_BOOLEAN VIR_DOMAIN_SCHED_FIELD_BOOLEAN
#define VIR_TYPED_PARAM_STRING 7

#define VIR_TYPED_PARAM_FIELD_LENGTH 80
typedef struct _virTypedParameter virTypedParameter;
struct _virTypedParameter {
    char field[VIR_TYPED_PARAM_FIELD_LENGTH];  /* parameter name */
    int type;   /* parameter type, virTypedParameterType */
    union {
        int i;                      /* type is INT */
        unsigned int ui;            /* type is UINT */
        long long int l;            /* type is LLONG */
        unsigned long long int ul;  /* type is ULLONG */
        double d;                   /* type is DOUBLE */
        char b;                     /* type is BOOLEAN */
        char *s;                    /* type is STRING, may not be NULL */
    } value; /* parameter value */
};
typedef virTypedParameter *virTypedParameterPtr;

#endif

static VALUE c_domain;
static VALUE c_domain_info;
static VALUE c_domain_ifinfo;
static VALUE c_domain_security_label;
static VALUE c_domain_block_stats;
#if HAVE_TYPE_VIRDOMAINBLOCKINFOPTR
static VALUE c_domain_block_info;
#endif
#if HAVE_TYPE_VIRDOMAINMEMORYSTATPTR
static VALUE c_domain_memory_stats;
#endif
#if HAVE_TYPE_VIRDOMAINSNAPSHOTPTR
static VALUE c_domain_snapshot;
#endif
#if HAVE_TYPE_VIRDOMAINJOBINFOPTR
static VALUE c_domain_job_info;
#endif
static VALUE c_domain_vcpuinfo;
#if HAVE_VIRDOMAINGETCONTROLINFO
static VALUE c_domain_control_info;
#endif

static void domain_free(void *d)
{
    generic_free(Domain, d);
}

VALUE domain_new(virDomainPtr d, VALUE conn)
{
    return generic_new(c_domain, d, conn, domain_free);
}

virDomainPtr domain_get(VALUE d)
{
    generic_get(Domain, d);
}

/*
 * call-seq:
 *   dom.migrate(dconn, flags=0, dname=nil, uri=nil, bandwidth=0) -> Libvirt::Domain
 *
 * Call virDomainMigrate[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrate]
 * to migrate a domain from the host on this connection to the connection
 * referenced in dconn.
 */
static VALUE libvirt_dom_migrate(int argc, VALUE *argv, VALUE d)
{
    VALUE dconn, flags, dname_val, uri_val, bandwidth;
    virDomainPtr ddom = NULL;

    rb_scan_args(argc, argv, "14", &dconn, &flags, &dname_val, &uri_val,
                 &bandwidth);

    bandwidth = integer_default_if_nil(bandwidth, 0);
    flags = integer_default_if_nil(flags, 0);

    ddom = virDomainMigrate(domain_get(d), connect_get(dconn), NUM2ULONG(flags),
                            get_string_or_nil(dname_val),
                            get_string_or_nil(uri_val), NUM2ULONG(bandwidth));

    _E(ddom == NULL, create_error(e_Error, "virDomainMigrate", connect_get(d)));

    return domain_new(ddom, dconn);
}

#if HAVE_VIRDOMAINMIGRATETOURI
/*
 * call-seq:
 *   dom.migrate_to_uri(duri, flags=0, dname=nil, bandwidth=0) -> nil
 *
 * Call virDomainMigrateToURI[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrateToURI]
 * to migrate a domain from the host on this connection to the host whose
 * libvirt URI is duri.
 */
static VALUE libvirt_dom_migrate_to_uri(int argc, VALUE *argv, VALUE d)
{
    VALUE duri, flags, dname, bandwidth;

    rb_scan_args(argc, argv, "13", &duri, &flags, &dname, &bandwidth);

    bandwidth = integer_default_if_nil(bandwidth, 0);
    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainMigrateToURI, connect_get(d), domain_get(d),
                  StringValueCStr(duri), NUM2ULONG(flags),
                  get_string_or_nil(dname), NUM2ULONG(bandwidth));
}
#endif

#if HAVE_VIRDOMAINMIGRATESETMAXDOWNTIME
/*
 * call-seq:
 *   dom.migrate_set_max_downtime(downtime, flags=0) -> nil
 *
 * Call virDomainMigrateSetMaxDowntime[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrateSetMaxDowntime]
 * to set the maximum downtime desired for live migration.
 */
static VALUE libvirt_dom_migrate_set_max_downtime(int argc, VALUE *argv,
                                                  VALUE d)
{
    VALUE downtime, flags;

    rb_scan_args(argc, argv, "11", &downtime, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainMigrateSetMaxDowntime, connect_get(d), domain_get(d),
                  NUM2ULL(downtime), NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINMIGRATE2
/*
 * call-seq:
 *   dom.migrate2(dconn, dxml=nil, flags=0, dname=nil, uri=nil, bandwidth=0) -> Libvirt::Domain
 *
 * Call virDomainMigrate2[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrate2]
 * to migrate a domain from the host on this connection to the connection
 * referenced in dconn.
 */
static VALUE libvirt_dom_migrate2(int argc, VALUE *argv, VALUE d)
{
    VALUE dconn, dxml, flags, dname_val, uri_val, bandwidth;
    virDomainPtr ddom = NULL;

    rb_scan_args(argc, argv, "15", &dconn, &dxml, &flags, &dname_val, &uri_val,
                 &bandwidth);

    bandwidth = integer_default_if_nil(bandwidth, 0);
    flags = integer_default_if_nil(flags, 0);

    ddom = virDomainMigrate2(domain_get(d), connect_get(dconn),
                             get_string_or_nil(dxml), NUM2ULONG(flags),
                             get_string_or_nil(dname_val),
                             get_string_or_nil(uri_val), NUM2ULONG(bandwidth));

    _E(ddom == NULL, create_error(e_Error, "virDomainMigrate2",
                                  connect_get(d)));

    return domain_new(ddom, dconn);
}

/*
 * call-seq:
 *   dom.migrate_to_uri2(duri=nil, migrate_uri=nil, dxml=nil, flags=0, dname=nil, bandwidth=0) -> nil
 *
 * Call virDomainMigrateToURI2[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrateToURI2]
 * to migrate a domain from the host on this connection to the host whose
 * libvirt URI is duri.
 */
static VALUE libvirt_dom_migrate_to_uri2(int argc, VALUE *argv, VALUE d)
{
    VALUE duri, migrate_uri, dxml, flags, dname, bandwidth;

    rb_scan_args(argc, argv, "06", &duri, &migrate_uri, &dxml, &flags, &dname,
                 &bandwidth);

    bandwidth = integer_default_if_nil(bandwidth, 0);
    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainMigrateToURI2, connect_get(d), domain_get(d),
                  get_string_or_nil(duri), get_string_or_nil(migrate_uri),
                  get_string_or_nil(dxml), NUM2ULONG(flags),
                  get_string_or_nil(dname), NUM2ULONG(bandwidth));
}

/*
 * call-seq:
 *   dom.migrate_set_max_speed(bandwidth, flags=0) -> nil
 *
 * Call virDomainMigrateSetMaxSpeed[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrateSetMaxSpeed]
 * to set the maximum bandwidth allowed for live migration.
 */
static VALUE libvirt_dom_migrate_set_max_speed(int argc, VALUE *argv, VALUE d)
{
    VALUE bandwidth, flags;

    rb_scan_args(argc, argv, "11", &bandwidth, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainMigrateSetMaxSpeed, connect_get(d), domain_get(d),
                  NUM2ULONG(bandwidth), NUM2UINT(flags));
}
#endif

/*
 * call-seq:
 *   dom.shutdown(flags=0) -> nil
 *
 * Call virDomainShutdown[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainShutdown]
 * to do a soft shutdown of the domain.  The mechanism for doing the shutdown
 * is hypervisor specific, and may require software running inside the domain
 * to succeed.
 */
static VALUE libvirt_dom_shutdown(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINSHUTDOWNFLAGS
    gen_call_void(virDomainShutdownFlags, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }

    gen_call_void(virDomainShutdown, connect_get(d), domain_get(d));
#endif
}

/*
 * call-seq:
 *   dom.reboot(flags=0) -> nil
 *
 * Call virDomainReboot[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainReboot]
 * to do a reboot of the domain.
 */
static VALUE libvirt_dom_reboot(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainReboot, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
}

/*
 * call-seq:
 *   dom.destroy(flags=0) -> nil
 *
 * Call virDomainDestroy[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainDestroy]
 * to do a hard power-off of the domain.
 */
static VALUE libvirt_dom_destroy(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINDESTROYFLAGS
    gen_call_void(virDomainDestroyFlags, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    gen_call_void(virDomainDestroy, connect_get(d), domain_get(d));
#endif
}

/*
 * call-seq:
 *   dom.suspend -> nil
 *
 * Call virDomainSuspend[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSuspend]
 * to stop the domain from executing.  The domain will still continue to
 * consume memory, but will not take any CPU time.
 */
static VALUE libvirt_dom_suspend(VALUE d)
{
    gen_call_void(virDomainSuspend, connect_get(d), domain_get(d));
}

/*
 * call-seq:
 *   dom.resume -> nil
 *
 * Call virDomainResume[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainResume]
 * to resume a suspended domain.  After this call the domain will start
 * consuming CPU resources again.
 */
static VALUE libvirt_dom_resume(VALUE d)
{
    gen_call_void(virDomainResume, connect_get(d), domain_get(d));
}

/*
 * call-seq:
 *   dom.save(filename, dxml=nil, flags=0) -> nil
 *
 * Call virDomainSave[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSave]
 * to save the domain state to filename.  After this call, the domain will no
 * longer be consuming any resources.
 */
static VALUE libvirt_dom_save(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;
    VALUE to;
    VALUE dxml;

    rb_scan_args(argc, argv, "12", &to, &dxml, &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINSAVEFLAGS
    gen_call_void(virDomainSaveFlags, connect_get(d), domain_get(d),
                  StringValueCStr(to), get_string_or_nil(dxml),
                  NUM2UINT(flags));
#else
    if (TYPE(dxml) != T_NIL) {
        rb_raise(e_NoSupportError, "Non-nil dxml not supported");
    }
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    gen_call_void(virDomainSave, connect_get(d), domain_get(d),
                  StringValueCStr(to));
#endif
}

#if HAVE_VIRDOMAINMANAGEDSAVE
/*
 * call-seq:
 *   dom.managed_save(flags=0) -> nil
 *
 * Call virDomainManagedSave[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainManagedSave]
 * to do a managed save of the domain.  The domain will be saved to a place
 * of libvirt's choosing.
 */
static VALUE libvirt_dom_managed_save(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainManagedSave, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
}

/*
 * call-seq:
 *   dom.has_managed_save?(flags=0) -> [True|False]
 *
 * Call virDomainHasManagedSaveImage[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainHasManagedSaveImage]
 * to determine if a particular domain has a managed save image.
 */
static VALUE libvirt_dom_has_managed_save(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_truefalse(virDomainHasManagedSaveImage, connect_get(d),
                       domain_get(d), NUM2UINT(flags));
}

/*
 * call-seq:
 *   dom.managed_save_remove(flags=0) -> nil
 *
 * Call virDomainManagedSaveRemove[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainManagedSaveRemove]
 * to remove the managed save image for a domain.
 */
static VALUE libvirt_dom_managed_save_remove(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainManagedSaveRemove, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
}
#endif

/*
 * call-seq:
 *   dom.core_dump(filename, flags=0) -> nil
 *
 * Call virDomainCoreDump[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainCoreDump]
 * to do a full memory dump of the domain to filename.
 */
static VALUE libvirt_dom_core_dump(int argc, VALUE *argv, VALUE d)
{
    VALUE to, flags;

    rb_scan_args(argc, argv, "11", &to, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainCoreDump, connect_get(d), domain_get(d),
                  StringValueCStr(to), NUM2INT(flags));
}

/*
 * call-seq:
 *   Libvirt::Domain::restore(conn, filename) -> nil
 *
 * Call virDomainRestore[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainRestore]
 * to restore the domain from the filename.
 */
static VALUE libvirt_dom_s_restore(VALUE klass, VALUE c, VALUE from)
{
    gen_call_void(virDomainRestore, connect_get(c), connect_get(c),
                  StringValueCStr(from));
}

/*
 * call-seq:
 *   dom.info -> Libvirt::Domain::Info
 *
 * Call virDomainGetInfo[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetInfo]
 * to retrieve domain information.
 */
static VALUE libvirt_dom_info(VALUE d)
{
    virDomainInfo info;
    int r;
    VALUE result;

    r = virDomainGetInfo(domain_get(d), &info);
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetInfo",
                           connect_get(d)));

    result = rb_class_new_instance(0, NULL, c_domain_info);
    rb_iv_set(result, "@state", CHR2FIX(info.state));
    rb_iv_set(result, "@max_mem", ULONG2NUM(info.maxMem));
    rb_iv_set(result, "@memory", ULONG2NUM(info.memory));
    rb_iv_set(result, "@nr_virt_cpu", INT2NUM((int) info.nrVirtCpu));
    rb_iv_set(result, "@cpu_time", ULL2NUM(info.cpuTime));

    return result;
}

#if HAVE_VIRDOMAINGETSECURITYLABEL
/*
 * call-seq:
 *   dom.security_label -> Libvirt::Domain::SecurityLabel
 *
 * Call virDomainGetSecurityLabel[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetSecurityLabel]
 * to retrieve the security label applied to this domain.
 */
static VALUE libvirt_dom_security_label(VALUE d)
{
    virSecurityLabel seclabel;
    int r;
    VALUE result;

    r = virDomainGetSecurityLabel(domain_get(d), &seclabel);
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetSecurityLabel",
                           connect_get(d)));

    result = rb_class_new_instance(0, NULL, c_domain_security_label);
    rb_iv_set(result, "@label", rb_str_new2(seclabel.label));
    rb_iv_set(result, "@enforcing", INT2NUM(seclabel.enforcing));

    return result;
}
#endif

/*
 * call-seq:
 *   dom.block_stats(path) -> Libvirt::Domain::BlockStats
 *
 * Call virDomainBlockStats[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainBlockStats]
 * to retrieve statistics about domain block device path.
 */
static VALUE libvirt_dom_block_stats(VALUE d, VALUE path)
{
    virDomainBlockStatsStruct stats;
    int r;
    VALUE result;

    r = virDomainBlockStats(domain_get(d), StringValueCStr(path), &stats,
                            sizeof(stats));
    _E(r < 0, create_error(e_RetrieveError, "virDomainBlockStats",
                           connect_get(d)));

    result = rb_class_new_instance(0, NULL, c_domain_block_stats);
    rb_iv_set(result, "@rd_req", LL2NUM(stats.rd_req));
    rb_iv_set(result, "@rd_bytes", LL2NUM(stats.rd_bytes));
    rb_iv_set(result, "@wr_req", LL2NUM(stats.wr_req));
    rb_iv_set(result, "@wr_bytes", LL2NUM(stats.wr_bytes));
    rb_iv_set(result, "@errs", LL2NUM(stats.errs));

    return result;
}

#if HAVE_TYPE_VIRDOMAINMEMORYSTATPTR
/*
 * call-seq:
 *   dom.memory_stats(flags=0) -> [ Libvirt::Domain::MemoryStats ]
 *
 * Call virDomainMemoryStats[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMemoryStats]
 * to retrieve statistics about the amount of memory consumed by a domain.
 */
static VALUE libvirt_dom_memory_stats(int argc, VALUE *argv, VALUE d)
{
    virDomainMemoryStatStruct stats[6];
    int r;
    VALUE result;
    VALUE flags;
    VALUE tmp;
    int i;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    r = virDomainMemoryStats(domain_get(d), stats, 6, NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainMemoryStats",
                           connect_get(d)));

    /* FIXME: the right rubyish way to have done this would have been to
     * create a hash with the values, something like:
     *
     * { 'SWAP_IN' => 0, 'SWAP_OUT' => 98, 'MAJOR_FAULT' => 45,
     *   'MINOR_FAULT' => 55, 'UNUSED' => 455, 'AVAILABLE' => 98 }
     *
     * Unfortunately this has already been released with the array version
     * so we have to maintain compatibility with that.  We should probably add
     * a new memory_stats-like call that properly creates the hash.
     */
    result = rb_ary_new2(r);
    for (i = 0; i < r; i++) {
        tmp = rb_class_new_instance(0, NULL, c_domain_memory_stats);
        rb_iv_set(tmp, "@tag", INT2NUM(stats[i].tag));
        rb_iv_set(tmp, "@val", ULL2NUM(stats[i].val));

        rb_ary_push(result, tmp);
    }                                           \

    return result;
}
#endif

#if HAVE_TYPE_VIRDOMAINBLOCKINFOPTR
/*
 * call-seq:
 *   dom.blockinfo(path, flags=0) -> Libvirt::Domain::BlockInfo
 *
 * Call virDomainGetBlockInfo[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetBlockInfo]
 * to retrieve information about the backing file path for the domain.
 */
static VALUE libvirt_dom_block_info(int argc, VALUE *argv, VALUE d)
{
    virDomainBlockInfo info;
    int r;
    VALUE result;
    VALUE flags;
    VALUE path;

    rb_scan_args(argc, argv, "11", &path, &flags);

    flags = integer_default_if_nil(flags, 0);

    r = virDomainGetBlockInfo(domain_get(d), StringValueCStr(path), &info,
                              NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetBlockInfo",
                           connect_get(d)));

    result = rb_class_new_instance(0, NULL, c_domain_block_info);
    rb_iv_set(result, "@capacity", ULL2NUM(info.capacity));
    rb_iv_set(result, "@allocation", ULL2NUM(info.allocation));
    rb_iv_set(result, "@physical", ULL2NUM(info.physical));

    return result;
}
#endif

#if HAVE_VIRDOMAINBLOCKPEEK
/*
 * call-seq:
 *   dom.block_peek(path, offset, size, flags=0) -> string
 *
 * Call virDomainBlockPeek[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainBlockPeek]
 * to read size number of bytes, starting at offset offset from domain backing
 * file path.  Due to limitations of the libvirt remote protocol, the user
 * should never request more than 64k bytes.
 */
static VALUE libvirt_dom_block_peek(int argc, VALUE *argv, VALUE d)
{
    VALUE path, offset, size, flags;
    char *buffer;
    int r;

    rb_scan_args(argc, argv, "31", &path, &offset, &size, &flags);

    flags = integer_default_if_nil(flags, 0);

    buffer = alloca(sizeof(char) * NUM2UINT(size));

    r = virDomainBlockPeek(domain_get(d), StringValueCStr(path),
                           NUM2ULL(offset), NUM2UINT(size), buffer,
                           NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainBlockPeek",
                           connect_get(d)));

    return rb_str_new(buffer, NUM2UINT(size));
}
#endif

#if HAVE_VIRDOMAINMEMORYPEEK
/*
 * call-seq:
 *   dom.memory_peek(start, size, flags=Libvirt::Domain::MEMORY_VIRTUAL) -> string
 *
 * Call virDomainMemoryPeek[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMemoryPeek]
 * to read size number of bytes from offset start from the domain memory.
 * Due to limitations of the libvirt remote protocol, the user
 * should never request more than 64k bytes.
 */
static VALUE libvirt_dom_memory_peek(int argc, VALUE *argv, VALUE d)
{
    VALUE start, size, flags;
    char *buffer;
    int r;

    rb_scan_args(argc, argv, "21", &start, &size, &flags);

    flags = integer_default_if_nil(flags, VIR_MEMORY_VIRTUAL);

    buffer = alloca(sizeof(char) * NUM2UINT(size));

    r = virDomainMemoryPeek(domain_get(d), NUM2ULL(start), NUM2UINT(size),
                            buffer, NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainMemoryPeek",
                           connect_get(d)));

    return rb_str_new(buffer, NUM2UINT(size));
}
#endif

/* call-seq:
 *   dom.get_vcpus -> [ Libvirt::Domain::VCPUInfo ]
 *
 * Call virDomainGetVcpus[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetVcpus]
 * to retrieve detailed information about the state of a domain's virtual CPUs.
 */
static VALUE libvirt_dom_get_vcpus(VALUE d)
{
    virNodeInfo nodeinfo;
    virDomainInfo dominfo;
    virVcpuInfoPtr cpuinfo = NULL;
    unsigned char *cpumap;
    int cpumaplen;
    int r;
    VALUE result;
    unsigned short i;
    int j;
    VALUE vcpuinfo;
    VALUE p2vcpumap;

    r = virNodeGetInfo(connect_get(d), &nodeinfo);
    _E(r < 0, create_error(e_RetrieveError, "virNodeGetInfo", connect_get(d)));

    r = virDomainGetInfo(domain_get(d), &dominfo);
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetInfo",
                           connect_get(d)));

    cpuinfo = alloca(sizeof(virVcpuInfo) * dominfo.nrVirtCpu);

    cpumaplen = VIR_CPU_MAPLEN(VIR_NODEINFO_MAXCPUS(nodeinfo));

    cpumap = alloca(dominfo.nrVirtCpu * cpumaplen);

    r = virDomainGetVcpus(domain_get(d), cpuinfo, dominfo.nrVirtCpu, cpumap,
                          cpumaplen);
    if (r < 0) {
#if HAVE_VIRDOMAINGETVCPUPININFO
        /* if the domain is not shutoff, then this is an error */
        if (dominfo.state != VIR_DOMAIN_SHUTOFF) {
            rb_exc_raise(create_error(e_RetrieveError, "virDomainGetVcpus",
                                      connect_get(d)));
        }

        /* otherwise, we can try to call virDomainGetVcpuPinInfo to get the
         * information instead
         */
        r = virDomainGetVcpuPinInfo(domain_get(d), dominfo.nrVirtCpu,
                                    cpumap, cpumaplen,
                                    VIR_DOMAIN_AFFECT_CONFIG);
        _E(r < 0, create_error(e_RetrieveError, "virDomainGetVcpuPinInfo",
                               connect_get(d)));

#else
        rb_exc_raise(create_error(e_RetrieveError, "virDomainGetVcpus",
                                  connect_get(d)));
#endif
    }

    result = rb_ary_new();

    for (i = 0; i < dominfo.nrVirtCpu; i++) {
        vcpuinfo = rb_class_new_instance(0, NULL, c_domain_vcpuinfo);
        rb_iv_set(vcpuinfo, "@number", UINT2NUM(i));
        if (cpuinfo != NULL) {
            rb_iv_set(vcpuinfo, "@state", INT2NUM(cpuinfo[i].state));
            rb_iv_set(vcpuinfo, "@cpu_time", ULL2NUM(cpuinfo[i].cpuTime));
            rb_iv_set(vcpuinfo, "@cpu", INT2NUM(cpuinfo[i].cpu));
        }
        else {
            rb_iv_set(vcpuinfo, "@state", Qnil);
            rb_iv_set(vcpuinfo, "@cpu_time", Qnil);
            rb_iv_set(vcpuinfo, "@cpu", Qnil);
        }

        p2vcpumap = rb_ary_new();

        for (j = 0; j < VIR_NODEINFO_MAXCPUS(nodeinfo); j++)
            rb_ary_push(p2vcpumap, (VIR_CPU_USABLE(cpumap,
                                                   VIR_CPU_MAPLEN(VIR_NODEINFO_MAXCPUS(nodeinfo)), i, j)) ? Qtrue : Qfalse);
        rb_iv_set(vcpuinfo, "@cpumap", p2vcpumap);

        rb_ary_push(result, vcpuinfo);
    }

    return result;
}

#if HAVE_VIRDOMAINISACTIVE
/*
 * call-seq:
 *   dom.active? -> [true|false]
 *
 * Call virDomainIsActive[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainIsActive]
 * to determine if this domain is currently active.
 */
static VALUE libvirt_dom_active_p(VALUE d)
{
    gen_call_truefalse(virDomainIsActive, connect_get(d), domain_get(d));
}
#endif

#if HAVE_VIRDOMAINISPERSISTENT
/*
 * call-seq:
 *   dom.persistent? -> [true|false]
 *
 * Call virDomainIsPersistent[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainIsPersistent]
 * to determine if this is a persistent domain.
 */
static VALUE libvirt_dom_persistent_p(VALUE d)
{
    gen_call_truefalse(virDomainIsPersistent, connect_get(d), domain_get(d));
}
#endif

/*
 * call-seq:
 *   dom.ifinfo(if) -> Libvirt::Domain::IfInfo
 *
 * Call virDomainInterfaceStats[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainInterfaceStats]
 * to retrieve statistics about domain interface if.
 */
static VALUE libvirt_dom_if_stats(VALUE d, VALUE sif)
{
    char *ifname = get_string_or_nil(sif);
    virDomainInterfaceStatsStruct ifinfo;
    int r;
    VALUE result = Qnil;

    if (ifname) {
        r = virDomainInterfaceStats(domain_get(d), ifname, &ifinfo,
                                    sizeof(virDomainInterfaceStatsStruct));
        _E(r < 0, create_error(e_RetrieveError, "virDomainInterfaceStats",
                               connect_get(d)));

        result = rb_class_new_instance(0, NULL, c_domain_ifinfo);
        rb_iv_set(result, "@rx_bytes", LL2NUM(ifinfo.rx_bytes));
        rb_iv_set(result, "@rx_packets", LL2NUM(ifinfo.rx_packets));
        rb_iv_set(result, "@rx_errs", LL2NUM(ifinfo.rx_errs));
        rb_iv_set(result, "@rx_drop", LL2NUM(ifinfo.rx_drop));
        rb_iv_set(result, "@tx_bytes", LL2NUM(ifinfo.tx_bytes));
        rb_iv_set(result, "@tx_packets", LL2NUM(ifinfo.tx_packets));
        rb_iv_set(result, "@tx_errs", LL2NUM(ifinfo.tx_errs));
        rb_iv_set(result, "@tx_drop", LL2NUM(ifinfo.tx_drop));
    }
    return result;
}

/*
 * call-seq:
 *   dom.name -> string
 *
 * Call virDomainGetName[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetName]
 * to retrieve the name of this domain.
 */
static VALUE libvirt_dom_name(VALUE d)
{
    gen_call_string(virDomainGetName, connect_get(d), 0, domain_get(d));
}

/*
 * call-seq:
 *   dom.id -> fixnum
 *
 * Call virDomainGetID[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetID]
 * to retrieve the ID of this domain.  If the domain isn't running, this will
 * be -1.
 */
static VALUE libvirt_dom_id(VALUE d)
{
    unsigned int id;
    int out;

    id = virDomainGetID(domain_get(d));

    /* we need to cast the unsigned int id to a signed int out to handle the
     * -1 case
     */
    out = id;
    _E(out == -1, create_error(e_RetrieveError, "virDomainGetID",
                               connect_get(d)));

    return INT2NUM(out);
}

/*
 * call-seq:
 *   dom.uuid -> string
 *
 * Call virDomainGetUUIDString[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetUUIDString]
 * to retrieve the UUID of this domain.
 */
static VALUE libvirt_dom_uuid(VALUE d)
{
    char uuid[VIR_UUID_STRING_BUFLEN];
    int r;

    r = virDomainGetUUIDString(domain_get(d), uuid);
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetUUIDString",
                           connect_get(d)));

    return rb_str_new2((char *) uuid);
}

/*
 * call-seq:
 *   dom.os_type -> string
 *
 * Call virDomainGetOSType[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetOSType]
 * to retrieve the os_type of this domain.  In libvirt terms, os_type determines
 * whether this domain is fully virtualized, paravirtualized, or a container.
 */
static VALUE libvirt_dom_os_type(VALUE d)
{
    gen_call_string(virDomainGetOSType, connect_get(d), 1, domain_get(d));
}

/*
 * call-seq:
 *   dom.max_memory -> fixnum
 *
 * Call virDomainGetMaxMemory[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetMaxMemory]
 * to retrieve the maximum amount of memory this domain is allowed to access.
 * Note that the current amount of memory this domain is allowed to access may
 * be different (see dom.memory_set).
 */
static VALUE libvirt_dom_max_memory(VALUE d)
{
    unsigned long max_memory;

    max_memory = virDomainGetMaxMemory(domain_get(d));
    _E(max_memory == 0, create_error(e_RetrieveError, "virDomainGetMaxMemory",
                                     connect_get(d)));

    return ULONG2NUM(max_memory);
}

/*
 * call-seq:
 *   dom.max_memory = Fixnum
 *
 * Call virDomainSetMaxMemory[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetMaxMemory]
 * to set the maximum amount of memory (in kilobytes) this domain should be
 * allowed to access.
 */
static VALUE libvirt_dom_max_memory_set(VALUE d, VALUE max_memory)
{
    int r;

    r = virDomainSetMaxMemory(domain_get(d), NUM2ULONG(max_memory));
    _E(r < 0, create_error(e_DefinitionError, "virDomainSetMaxMemory",
                           connect_get(d)));

    return ULONG2NUM(max_memory);
}

/*
 * call-seq:
 *   dom.memory = Fixnum,flags=0
 *
 * Call virDomainSetMemory[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetMemory]
 * to set the amount of memory (in kilobytes) this domain should currently
 * have.  Note this will only succeed if both the hypervisor and the domain on
 * this connection support ballooning.
 */
static VALUE libvirt_dom_memory_set(VALUE d, VALUE in)
{
    VALUE memory;
    VALUE flags;
    int r;

    if (TYPE(in) == T_FIXNUM) {
        memory = in;
        flags = INT2NUM(0);
    }
    else if (TYPE(in) == T_ARRAY) {
        if (RARRAY_LEN(in) != 2) {
            rb_raise(rb_eArgError, "wrong number of arguments (%ld for 2)",
                     RARRAY_LEN(in));
        }
        memory = rb_ary_entry(in, 0);
        flags = rb_ary_entry(in, 1);
    }
    else {
        rb_raise(rb_eTypeError,
                 "wrong argument type (expected Number or Array)");
    }

#if HAVE_VIRDOMAINSETMEMORYFLAGS
    r = virDomainSetMemoryFlags(domain_get(d), NUM2ULONG(memory),
                                NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    r = virDomainSetMemory(domain_get(d), NUM2ULONG(memory));
#endif

    _E(r < 0, create_error(e_DefinitionError, "virDomainSetMemory",
                           connect_get(d)));

    return ULONG2NUM(memory);
}

/*
 * call-seq:
 *   dom.max_vcpus -> fixnum
 *
 * Call virDomainGetMaxVcpus[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetMaxVcpus]
 * to retrieve the maximum number of virtual CPUs this domain can use.
 */
static VALUE libvirt_dom_max_vcpus(VALUE d)
{
    gen_call_int(virDomainGetMaxVcpus, connect_get(d), domain_get(d));
}

#if HAVE_VIRDOMAINGETVCPUSFLAGS
/* call-seq:
 *   dom.num_vcpus(flags) -> fixnum
 *
 * Call virDomainGetVcpusFlags[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetVcpusFlags]
 * to retrieve the number of virtual CPUs assigned to this domain.
 */
static VALUE libvirt_dom_num_vcpus(VALUE d, VALUE flags)
{
    gen_call_int(virDomainGetVcpusFlags, connect_get(d), domain_get(d),
                 NUM2UINT(flags));
}
#endif

/*
 * call-seq:
 *   dom.vcpus = Fixnum,flags=0
 *
 * Call virDomainSetVcpus[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetVcpus]
 * to set the current number of virtual CPUs this domain should have.  Note
 * that this will only work if both the hypervisor and domain on this
 * connection support virtual CPU hotplug/hot-unplug.
 */
static VALUE libvirt_dom_vcpus_set(VALUE d, VALUE in)
{
    VALUE nvcpus;
    VALUE flags;

    if (TYPE(in) == T_FIXNUM) {
        nvcpus = in;
        flags = INT2NUM(0);
    }
#if HAVE_VIRDOMAINSETVCPUSFLAGS
    else if (TYPE(in) == T_ARRAY) {
        if (RARRAY_LEN(in) != 2) {
            rb_raise(rb_eArgError, "wrong number of arguments (%ld for 2)",
                     RARRAY_LEN(in));
        }
        nvcpus = rb_ary_entry(in, 0);
        flags = rb_ary_entry(in, 1);
    }
    else {
        rb_raise(rb_eTypeError,
                 "wrong argument type (expected Number or Array)");
    }

    gen_call_void(virDomainSetVcpusFlags, connect_get(d), domain_get(d),
                  NUM2UINT(nvcpus), NUM2UINT(flags));
#else

    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }

    gen_call_void(virDomainSetVcpus, connect_get(d), domain_get(d),
                  NUM2UINT(nvcpus));
#endif
}

#if HAVE_VIRDOMAINSETVCPUSFLAGS
/*
 * call-seq:
 *   dom.vcpus_flags = Fixnum,flags=0
 *
 * Call virDomainSetVcpusFlags[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetVcpusFlags]
 * to set the current number of virtual CPUs this domain should have.  The
 * flags parameter controls whether the change is made to the running domain
 * the domain configuration, or both, and must not be 0.
 */
static VALUE libvirt_dom_vcpus_set_flags(VALUE d, VALUE in)
{
    VALUE nvcpus;
    VALUE flags;

    if (TYPE(in) == T_FIXNUM) {
        nvcpus = in;
        flags = INT2NUM(0);
    }
    else if (TYPE(in) == T_ARRAY) {
        if (RARRAY_LEN(in) != 2) {
            rb_raise(rb_eArgError, "wrong number of arguments (%ld for 2)",
                     RARRAY_LEN(in));
        }
        nvcpus = rb_ary_entry(in, 0);
        flags = rb_ary_entry(in, 1);
    }
    else {
        rb_raise(rb_eTypeError,
                 "wrong argument type (expected Number or Array)");
    }

    gen_call_void(virDomainSetVcpusFlags, connect_get(d), domain_get(d),
                  NUM2UINT(nvcpus), NUM2UINT(flags));
}
#endif

/*
 * call-seq:
 *   dom.pin_vcpu(vcpu, cpulist, flags=0) -> nil
 *
 * Call virDomainPinVcpu[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainPinVcpu]
 * to pin a particular virtual CPU to a range of physical processors.  The
 * cpulist should be an array of Fixnums representing the physical processors
 * this virtual CPU should be allowed to be scheduled on.
 */
static VALUE libvirt_dom_pin_vcpu(int argc, VALUE *argv, VALUE d)
{
    VALUE vcpu, cpulist, flags;
    int r, i, len, maplen;
    unsigned char *cpumap;
    virNodeInfo nodeinfo;
    unsigned int vcpunum;
    VALUE e;

    rb_scan_args(argc, argv, "21", &vcpu, &cpulist, &flags);

    flags = integer_default_if_nil(flags, 0);

    vcpunum = NUM2UINT(vcpu);
    Check_Type(cpulist, T_ARRAY);

    r = virNodeGetInfo(connect_get(d), &nodeinfo);
    _E(r < 0, create_error(e_RetrieveError, "virNodeGetInfo", connect_get(d)));

    maplen = VIR_CPU_MAPLEN(nodeinfo.cpus);
    cpumap = alloca(sizeof(unsigned char) * maplen);
    MEMZERO(cpumap, unsigned char, maplen);

    len = RARRAY_LEN(cpulist);
    for(i = 0; i < len; i++) {
        e = rb_ary_entry(cpulist, i);
        VIR_USE_CPU(cpumap, NUM2UINT(e));
    }

#if HAVE_VIRDOMAINPINVCPUFLAGS
    gen_call_void(virDomainPinVcpuFlags, connect_get(d), domain_get(d),
                  vcpunum, cpumap, maplen, NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }

    gen_call_void(virDomainPinVcpu, connect_get(d), domain_get(d), vcpunum,
                  cpumap, maplen);
#endif
}

/*
 * call-seq:
 *   dom.xml_desc(flags=0) -> string
 *
 * Call virDomainGetXMLDesc[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetXMLDesc]
 * to retrieve the XML describing this domain.
 */
static VALUE libvirt_dom_xml_desc(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_string(virDomainGetXMLDesc, connect_get(d), 1, domain_get(d),
                    NUM2INT(flags));
}

/*
 * call-seq:
 *   dom.undefine(flags=0) -> nil
 *
 * Call virDomainUndefine[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainUndefine]
 * to undefine the domain.  After this call, the domain object is no longer
 * valid.
 */
static VALUE libvirt_dom_undefine(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINUNDEFINEFLAGS
    gen_call_void(virDomainUndefineFlags, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }

    gen_call_void(virDomainUndefine, connect_get(d), domain_get(d));
#endif
}

/*
 * call-seq:
 *   dom.create(flags=0) -> nil
 *
 * Call virDomainCreate[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainCreate]
 * to start an already defined domain.
 */
static VALUE libvirt_dom_create(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINCREATEWITHFLAGS
    gen_call_void(virDomainCreateWithFlags, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    gen_call_void(virDomainCreate, connect_get(d), domain_get(d));
#endif
}

/*
 * call-seq:
 *   dom.autostart -> [true|false]
 *
 * Call virDomainGetAutostart[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetAutostart]
 * to find out the state of the autostart flag for a domain.
 */
static VALUE libvirt_dom_autostart(VALUE d)
{
    int r, autostart;

    r = virDomainGetAutostart(domain_get(d), &autostart);
    _E(r < 0, create_error(e_RetrieveError, "virDomainAutostart",
                           connect_get(d)));

    return autostart ? Qtrue : Qfalse;
}

/*
 * call-seq:
 *   dom.autostart = [true|false]
 *
 * Call virDomainSetAutostart[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetAutostart]
 * to make this domain autostart when libvirtd starts up.
 */
static VALUE libvirt_dom_autostart_set(VALUE d, VALUE autostart)
{
    if (autostart != Qtrue && autostart != Qfalse) {
		rb_raise(rb_eTypeError,
                 "wrong argument type (expected TrueClass or FalseClass)");
    }

    gen_call_void(virDomainSetAutostart, connect_get(d),
                  domain_get(d), RTEST(autostart) ? 1 : 0);
}

/*
 * call-seq:
 *   dom.attach_device(device_xml, flags=0) -> nil
 *
 * Call virDomainAttachDevice[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainAttachDevice]
 * to attach the device described by the device_xml to the domain.
 */
static VALUE libvirt_dom_attach_device(int argc, VALUE *argv, VALUE d)
{
    VALUE xml;
    VALUE flags;

    rb_scan_args(argc, argv, "11", &xml, &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINATTACHDEVICEFLAGS
    gen_call_void(virDomainAttachDeviceFlags, connect_get(d), domain_get(d),
                  StringValueCStr(xml), NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    gen_call_void(virDomainAttachDevice, connect_get(d), domain_get(d),
                  StringValueCStr(xml));
#endif
}

/*
 * call-seq:
 *   dom.detach_device(device_xml, flags=0) -> nil
 *
 * Call virDomainDetachDevice[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainDetachDevice]
 * to detach the device described by the device_xml from the domain.
 */
static VALUE libvirt_dom_detach_device(int argc, VALUE *argv, VALUE d)
{
    VALUE xml;
    VALUE flags;

    rb_scan_args(argc, argv, "11", &xml, &flags);

    flags = integer_default_if_nil(flags, 0);

#if HAVE_VIRDOMAINDETACHDEVICEFLAGS
    gen_call_void(virDomainDetachDeviceFlags, connect_get(d), domain_get(d),
                  StringValueCStr(xml), NUM2UINT(flags));
#else
    if (NUM2UINT(flags) != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    gen_call_void(virDomainDetachDevice, connect_get(d), domain_get(d),
                  StringValueCStr(xml));
#endif
}

#if HAVE_VIRDOMAINUPDATEDEVICEFLAGS
/*
 * call-seq:
 *   dom.update_device(device_xml, flags=0) -> nil
 *
 * Call virDomainUpdateDeviceFlags[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainUpdateDeviceFlags]
 * to update the device described by the device_xml.
 */
static VALUE libvirt_dom_update_device(int argc, VALUE *argv, VALUE d)
{
    VALUE xml;
    VALUE flags;

    rb_scan_args(argc, argv, "11", &xml, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainUpdateDeviceFlags, connect_get(d), domain_get(d),
                  StringValueCStr(xml), NUM2UINT(flags));
}
#endif

/*
 * call-seq:
 *   dom.free -> nil
 *
 * Call virDomainFree[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainFree]
 * to free a domain object.
 */
static VALUE libvirt_dom_free(VALUE d)
{
    gen_call_free(Domain, d);
}

#if HAVE_TYPE_VIRDOMAINSNAPSHOTPTR
static void domain_snapshot_free(void *d)
{
    generic_free(DomainSnapshot, d);
}

static VALUE domain_snapshot_new(virDomainSnapshotPtr d, VALUE domain)
{
    VALUE result;
    result = Data_Wrap_Struct(c_domain_snapshot, NULL, domain_snapshot_free, d);
    rb_iv_set(result, "@domain", domain);
    return result;
}

static virDomainSnapshotPtr domain_snapshot_get(VALUE d)
{
    generic_get(DomainSnapshot, d);
}

/*
 * call-seq:
 *   dom.snapshot_create_xml(snapshot_xml, flags=0) -> Libvirt::Domain::Snapshot
 *
 * Call virDomainSnapshotCreateXML[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotCreateXML]
 * to create a new snapshot based on snapshot_xml.
 */
static VALUE libvirt_dom_snapshot_create_xml(int argc, VALUE *argv, VALUE d)
{
    VALUE xmlDesc, flags;
    virDomainSnapshotPtr ret;

    rb_scan_args(argc, argv, "11", &xmlDesc, &flags);

    flags = integer_default_if_nil(flags, 0);

    ret = virDomainSnapshotCreateXML(domain_get(d), StringValueCStr(xmlDesc),
                                     NUM2UINT(flags));

    _E(ret == NULL, create_error(e_Error, "virDomainSnapshotCreateXML",
                                 connect_get(d)));

    return domain_snapshot_new(ret, d);
}

/*
 * call-seq:
 *   dom.num_of_snapshots(flags=0) -> fixnum
 *
 * Call virDomainSnapshotNum[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotNum]
 * to retrieve the number of available snapshots for this domain.
 */
static VALUE libvirt_dom_num_of_snapshots(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_int(virDomainSnapshotNum, connect_get(d), domain_get(d),
                 NUM2UINT(flags));
}

/*
 * call-seq:
 *   dom.list_snapshots(flags=0) -> list
 *
 * Call virDomainSnapshotListNames[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotListNames]
 * to retrieve a list of snapshot names available for this domain.
 */
static VALUE libvirt_dom_list_snapshots(int argc, VALUE *argv, VALUE d)
{
    VALUE flags_val;
    int r;
    int num;
    char **names;
    unsigned int flags;

    rb_scan_args(argc, argv, "01", &flags_val);

    if (NIL_P(flags_val)) {
        flags = 0;
    }
    else {
        flags = NUM2UINT(flags_val);
    }

    num = virDomainSnapshotNum(domain_get(d), 0);
    _E(num < 0, create_error(e_RetrieveError, "virDomainSnapshotNum",
                             connect_get(d)));
    if (num == 0) {
        /* if num is 0, don't call virDomainSnapshotListNames function */
        return rb_ary_new2(num);
    }

    names = alloca(sizeof(char *) * num);

    r = virDomainSnapshotListNames(domain_get(d), names, num, flags);
    _E(r < 0, create_error(e_RetrieveError, "virDomainSnapshotListNames",
                           connect_get(d)));

    return gen_list(num, names);
}

/*
 * call-seq:
 *   dom.lookup_snapshot_by_name(name, flags=0) -> Libvirt::Domain::Snapshot
 *
 * Call virDomainSnapshotLookupByName[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotLookupByName]
 * to retrieve a snapshot object corresponding to snapshot name.
 */
static VALUE libvirt_dom_lookup_snapshot_by_name(int argc, VALUE *argv, VALUE d)
{
    virDomainSnapshotPtr snap;
    VALUE name, flags;

    rb_scan_args(argc, argv, "11", &name, &flags);

    flags = integer_default_if_nil(flags, 0);

    snap = virDomainSnapshotLookupByName(domain_get(d), StringValueCStr(name),
                                         NUM2UINT(flags));
    _E(snap == NULL, create_error(e_RetrieveError,
                                  "virDomainSnapshotLookupByName",
                                  connect_get(d)));

    return domain_snapshot_new(snap, d);
}

/*
 * call-seq:
 *   dom.has_current_snapshot?(flags=0) -> [true|false]
 *
 * Call virDomainHasCurrentSnapshot[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainHasCurrentSnapshot]
 * to find out if this domain has a snapshot active.
 */
static VALUE libvirt_dom_has_current_snapshot_p(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_truefalse(virDomainHasCurrentSnapshot, connect_get(d),
                       domain_get(d), NUM2UINT(flags));
}

/*
 * call-seq:
 *   dom.revert_to_snapshot(snapshot_object, flags=0) -> nil
 *
 * Call virDomainRevertToSnapshot[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainRevertToSnapshot]
 * to restore this domain to a previously saved snapshot.
 */
static VALUE libvirt_dom_revert_to_snapshot(int argc, VALUE *argv, VALUE d)
{
    VALUE snap, flags;

    rb_scan_args(argc, argv, "11", &snap, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainRevertToSnapshot, connect_get(d),
                  domain_snapshot_get(snap), NUM2UINT(flags));
}

/*
 * call-seq:
 *   dom.current_snapshot(flags=0) -> Libvirt::Domain::Snapshot
 *
 * Call virDomainCurrentSnapshot[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainCurrentSnapshot]
 * to retrieve the current snapshot for this domain (if any).
 */
static VALUE libvirt_dom_current_snapshot(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;
    virDomainSnapshotPtr snap;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    snap = virDomainSnapshotCurrent(domain_get(d), NUM2UINT(flags));
    _E(snap == NULL, create_error(e_RetrieveError, "virDomainSnapshotCurrent",
                                  connect_get(d)));

    return domain_snapshot_new(snap, d);
}

/*
 * call-seq:
 *   snapshot.xml_desc(flags=0) -> string
 *
 * Call virDomainSnapshotGetXMLDesc[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotGetXMLDesc]
 * to retrieve the xml description for this snapshot.
 */
static VALUE libvirt_dom_snapshot_xml_desc(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_string(virDomainSnapshotGetXMLDesc, connect_get(d), 1,
                    domain_snapshot_get(d), NUM2UINT(flags));
}

/*
 * call-seq:
 *   snapshot.delete(flags=0) -> nil
 *
 * Call virDomainSnapshotDelete[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotDelete]
 * to delete this snapshot.
 */
static VALUE libvirt_dom_snapshot_delete(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainSnapshotDelete, connect_get(d),
                  domain_snapshot_get(d), NUM2UINT(flags));
}

/*
 * call-seq:
 *   snapshot.free -> nil
 *
 * Call virDomainSnapshotFree[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotFree]
 * to free up the snapshot object.  After this call the snapshot object is
 * no longer valid.
 */
static VALUE libvirt_dom_snapshot_free(VALUE d)
{
    gen_call_free(DomainSnapshot, d);
}

#endif

#if HAVE_VIRDOMAINSNAPSHOTGETNAME
/*
 * call-seq:
 *   snapshot.name -> string
 *
 * Call virDomainSnapshotGetName[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotGetName]
 * to get the name associated with a snapshot.
 */
static VALUE libvirt_dom_snapshot_name(VALUE d)
{
    gen_call_string(virDomainSnapshotGetName, connect_get(d), 0,
                    domain_snapshot_get(d));
}
#endif


#if HAVE_TYPE_VIRDOMAINJOBINFOPTR
/*
 * call-seq:
 *   dom.job_info -> Libvirt::Domain::JobInfo
 *
 * Call virDomainGetJobInfo[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetJobInfo]
 * to retrieve the current state of the running domain job.
 */
static VALUE libvirt_dom_job_info(VALUE d)
{
    int r;
    virDomainJobInfo info;
    VALUE result;

    r = virDomainGetJobInfo(domain_get(d), &info);
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetJobInfo",
                           connect_get(d)));

    result = rb_class_new_instance(0, NULL, c_domain_job_info);
    rb_iv_set(result, "@type", INT2NUM(info.type));
    rb_iv_set(result, "@time_elapsed", ULL2NUM(info.timeElapsed));
    rb_iv_set(result, "@time_remaining", ULL2NUM(info.timeRemaining));
    rb_iv_set(result, "@data_total", ULL2NUM(info.dataTotal));
    rb_iv_set(result, "@data_processed", ULL2NUM(info.dataProcessed));
    rb_iv_set(result, "@data_remaining", ULL2NUM(info.dataRemaining));
    rb_iv_set(result, "@mem_total", ULL2NUM(info.memTotal));
    rb_iv_set(result, "@mem_processed", ULL2NUM(info.memProcessed));
    rb_iv_set(result, "@mem_remaining", ULL2NUM(info.memRemaining));
    rb_iv_set(result, "@file_total", ULL2NUM(info.fileTotal));
    rb_iv_set(result, "@file_processed", ULL2NUM(info.fileProcessed));
    rb_iv_set(result, "@file_remaining", ULL2NUM(info.fileRemaining));

    return result;
}

/*
 * call-seq:
 *   dom.abort_job -> nil
 *
 * Call virDomainAbortJob[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainAbortJob]
 * to abort the currently running job on this domain.
 */
static VALUE libvirt_dom_abort_job(VALUE d)
{
    gen_call_void(virDomainAbortJob, connect_get(d), domain_get(d));
}

#endif

struct create_sched_type_args {
    char *type;
    int nparams;
};

static VALUE create_sched_type_array(VALUE input)
{
    struct create_sched_type_args *args;
    VALUE result;

    args = (struct create_sched_type_args *)input;

    result = rb_ary_new();
    rb_ary_push(result, rb_str_new2(args->type));
    rb_ary_push(result, INT2NUM(args->nparams));

    return result;
}

/*
 * call-seq:
 *   dom.scheduler_type -> [type, #params]
 *
 * Call virDomainGetSchedulerType[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetSchedulerType]
 * to retrieve the scheduler type used on this domain.
 */
static VALUE libvirt_dom_scheduler_type(VALUE d)
{
    int nparams;
    char *type;
    VALUE result;
    int exception = 0;
    struct create_sched_type_args args;

    type = virDomainGetSchedulerType(domain_get(d), &nparams);

    _E(type == NULL, create_error(e_RetrieveError, "virDomainGetSchedulerType",
                                  connect_get(d)));

    args.type = type;
    args.nparams = nparams;
    result = rb_protect(create_sched_type_array, (VALUE)&args, &exception);
    if (exception) {
        free(type);
        rb_jump_tag(exception);
    }

    return result;
}

#if HAVE_VIRDOMAINQEMUMONITORCOMMAND
/*
 * call-seq:
 *   dom.qemu_monitor_command(cmd, flags=0) -> string
 *
 * Call virDomainQemuMonitorCommand
 * to send a qemu command directly to the monitor.  Note that this will only
 * work on qemu hypervisors, and the input and output formats are not
 * guaranteed to be stable.  Also note that using this command can severly
 * impede libvirt's ability to manage the domain; use with caution!
 */
static VALUE libvirt_dom_qemu_monitor_command(int argc, VALUE *argv, VALUE d)
{
    VALUE cmd, flags;
    char *result;
    VALUE ret;
    int exception;
    const char *type;
    int r;

    rb_scan_args(argc, argv, "11", &cmd, &flags);

    flags = integer_default_if_nil(flags, 0);

    type = virConnectGetType(connect_get(d));
    _E(type == NULL, create_error(e_Error, "virConnectGetType",
                                  connect_get(d)));
    if (strcmp(type, "QEMU") != 0) {
        rb_raise(rb_eTypeError,
                 "Tried to use virDomainQemuMonitor command on %s connection",
                 type);
    }

    r = virDomainQemuMonitorCommand(domain_get(d), StringValueCStr(cmd),
                                    &result, NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainQemuMonitorCommand",
                           connect_get(d)));

    ret = rb_protect(rb_str_new2_wrap, (VALUE)&result, &exception);
    free(result);
    if (exception) {
        rb_jump_tag(exception);
    }

    return ret;
}
#endif

#if HAVE_VIRDOMAINISUPDATED
/*
 * call-seq:
 *   dom.updated? ->  [True|False]
 *
 * Call virDomainIsUpdated[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainIsUpdated]
 * to determine whether the definition for this domain has been updated.
 */
static VALUE libvirt_dom_is_updated(VALUE d)
{
    gen_call_truefalse(virDomainIsUpdated, connect_get(d), domain_get(d));
}
#endif

static int scheduler_nparams(VALUE d, unsigned int flags)
{
    int nparams;
    char *type;

    type = virDomainGetSchedulerType(domain_get(d), &nparams);
    _E(type == NULL, create_error(e_RetrieveError, "virDomainGetSchedulerType",
                                  connect_get(d)));
    xfree(type);

    return nparams;
}

static char *scheduler_get(VALUE d, unsigned int flags,
                           virTypedParameterPtr params, int *nparams)
{
#ifdef HAVE_TYPE_VIRTYPEDPARAMETERPTR
    if (virDomainGetSchedulerParametersFlags(domain_get(d), params, nparams,
                                             flags) < 0) {
        return "virDomainGetSchedulerParameters";
    }
#else
    if (flags != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    if (virDomainGetSchedulerParameters(domain_get(d),
                                        (virSchedParameterPtr)params,
                                        nparams) < 0) {
        return "virDomainGetSchedulerParameters";
    }
#endif

    return NULL;
}

static char *scheduler_set(VALUE d, unsigned int flags,
                           virTypedParameterPtr params, int nparams)
{
#if HAVE_TYPE_VIRTYPEDPARAMETERPTR
    /* FIXME: virDomainSetSchedulerParametersFlags can take a flags parameter,
     * so we should probably implement it and pass it through.
     */
    if (virDomainSetSchedulerParametersFlags(domain_get(d), params, nparams,
                                             flags) < 0) {
        return "virDomainSetSchedulerParameters";
    }
#else
    if (flags != 0) {
        rb_raise(e_NoSupportError, "Non-zero flags not supported");
    }
    if (virDomainSetSchedulerParameters(domain_get(d),
                                        (virSchedParameterPtr)params,
                                        nparams) < 0) {
        return "virDomainSetSchedulerParameters";
    }
#endif

    return NULL;
}

/*
 * call-seq:
 *   dom.scheduler_parameters(flags=0) -> Hash
 *
 * Call virDomainGetSchedulerParameters[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetSchedulerParameters]
 * to retrieve all of the scheduler parameters for this domain.  The keys and
 * values in the hash that is returned are hypervisor specific.
 */
static VALUE libvirt_dom_get_scheduler_parameters(int argc, VALUE *argv,
                                                  VALUE d)
{
    return get_parameters(argc, argv, d, connect_get(d), scheduler_nparams,
                          scheduler_get);
}

/*
 * call-seq:
 *   dom.scheduler_parameters = Hash
 *
 * Call virDomainSetSchedulerParameters[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetSchedulerParameters]
 * to set the scheduler parameters for this domain.  The keys and values in
 * the input hash are hypervisor specific.  If an empty hash is given, no
 * changes are made (and no error is raised).
 */
static VALUE libvirt_dom_set_scheduler_parameters(VALUE d, VALUE input)
{
    return set_parameters(d, input, connect_get(d), scheduler_nparams,
                          scheduler_get, scheduler_set);
}

#if HAVE_VIRDOMAINSETMEMORYPARAMETERS
static int memory_nparams(VALUE d, unsigned int flags)
{
    int nparams = 0;
    int ret;

    ret = virDomainGetMemoryParameters(domain_get(d), NULL, &nparams, flags);
    _E(ret < 0, create_error(e_RetrieveError, "virDomainGetMemoryParameters",
                             connect_get(d)));

    return nparams;
}

static char *memory_get(VALUE d, unsigned int flags,
                        virTypedParameterPtr params, int *nparams)
{
#ifdef HAVE_TYPE_VIRTYPEDPARAMETERPTR
    if (virDomainGetMemoryParameters(domain_get(d), params, nparams,
                                     flags) < 0) {
#else
    if (virDomainGetMemoryParameters(domain_get(d),
                                     (virMemoryParameterPtr)params, nparams,
                                     flags) < 0) {
#endif
        return "virDomainGetMemoryParameters";
    }

    return NULL;
}

static char *memory_set(VALUE d, unsigned int flags,
                        virTypedParameterPtr params, int nparams)
{
    /* FIXME: virDomainSetMemoryParameters can take a flags parameter, so we
     * should probably implement it and pass it through.
     */
#ifdef HAVE_TYPE_VIRTYPEDPARAMETERPTR
    if (virDomainSetMemoryParameters(domain_get(d), params, nparams,
                                     flags) < 0) {
#else
    if (virDomainSetMemoryParameters(domain_get(d),
                                     (virMemoryParameterPtr)params, nparams,
                                     flags) < 0) {
#endif
        return "virDomainSetMemoryParameters";
    }

    return NULL;
}

/*
 * call-seq:
 *   dom.memory_parameters(flags=0) -> Hash
 *
 * Call virDomainGetMemoryParameters[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetMemoryParameters]
 * to retrieve all of the memory parameters for this domain.  The keys and
 * values in the hash that is returned are hypervisor specific.
 */
static VALUE libvirt_dom_get_memory_parameters(int argc, VALUE *argv, VALUE d)
{
    return get_parameters(argc, argv, d, connect_get(d), memory_nparams,
                          memory_get);
}

/*
 * call-seq:
 *   dom.memory_parameters = Hash
 *
 * Call virDomainSetMemoryParameters[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetMemoryParameters]
 * to set the memory parameters for this domain.  The keys and values in
 * the input hash are hypervisor specific.
 */
static VALUE libvirt_dom_set_memory_parameters(VALUE d, VALUE in)
{
    return set_parameters(d, in, connect_get(d), memory_nparams, memory_get,
                          memory_set);
}
#endif

#if HAVE_VIRDOMAINSETBLKIOPARAMETERS
static int blkio_nparams(VALUE d, unsigned int flags)
{
    int nparams = 0;
    int ret;

    ret = virDomainGetBlkioParameters(domain_get(d), NULL, &nparams, flags);
    _E(ret < 0, create_error(e_RetrieveError, "virDomainGetBlkioParameters",
                             connect_get(d)));

    return nparams;
}

static char *blkio_get(VALUE d, unsigned int flags, virTypedParameterPtr params,
                       int *nparams)
{
#ifdef HAVE_TYPE_VIRTYPEDPARAMETERPTR
    if (virDomainGetBlkioParameters(domain_get(d), params, nparams,
                                    flags) < 0) {
#else
    if (virDomainGetBlkioParameters(domain_get(d),
                                    (virBlkioParameterPtr)params, nparams,
                                    flags) < 0) {
#endif
        return "virDomainGetBlkioParameters";
    }

    return NULL;
}

static char *blkio_set(VALUE d, unsigned int flags, virTypedParameterPtr params,
                       int nparams)
{
    /* FIXME: virDomainSetBlkioParameters can take a flags parameter, so we
     * should probably implement it and pass it through.
     */
#ifdef HAVE_TYPE_VIRTYPEDPARAMETERPTR
    if (virDomainSetBlkioParameters(domain_get(d), params, nparams,
                                    flags) < 0) {
#else
    if (virDomainSetBlkioParameters(domain_get(d),
                                    (virBlkioParameterPtr)params, nparams,
                                    flags) < 0) {
#endif
        return "virDomainSetBlkioParameters";
    }

    return NULL;
}

/*
 * call-seq:
 *   dom.blkio_parameters(flags=0) -> Hash
 *
 * Call virDomainGetBlkioParameters[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetBlkioParameters]
 * to retrieve all of the blkio parameters for this domain.  The keys and
 * values in the hash that is returned are hypervisor specific.
 */
static VALUE libvirt_dom_get_blkio_parameters(int argc, VALUE *argv, VALUE d)
{
    return get_parameters(argc, argv, d, connect_get(d), blkio_nparams,
                          blkio_get);
}

/*
 * call-seq:
 *   dom.blkio_parameters = Hash
 *
 * Call virDomainSetBlkioParameters[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetBlkioParameters]
 * to set the blkio parameters for this domain.  The keys and values in
 * the input hash are hypervisor specific.
 */
static VALUE libvirt_dom_set_blkio_parameters(VALUE d, VALUE in)
{
    return set_parameters(d, in, connect_get(d), blkio_nparams, blkio_get,
                          blkio_set);
}
#endif

#if HAVE_VIRDOMAINGETSTATE
/*
 * call-seq:
 *   dom.state(flags=0) -> state, reason
 *
 * Call virDomainGetState[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetState]
 * to get the current state of the domain.
 */
static VALUE libvirt_dom_get_state(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;
    int state, reason;
    VALUE result;
    int retval;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    retval = virDomainGetState(domain_get(d), &state, &reason, NUM2INT(flags));
    _E(retval < 0, create_error(e_Error, "virDomainGetState", connect_get(d)));

    result = rb_ary_new();

    rb_ary_push(result, INT2NUM(state));
    rb_ary_push(result, INT2NUM(reason));

    return result;
}
#endif

#if HAVE_VIRDOMAINOPENCONSOLE
/*
 * call-seq:
 *   dom.open_console(device, stream, flags=0) -> nil
 *
 * Call virDomainOpenConsole[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainOpenConsole]
 * to open up a console to device over stream.
 */
static VALUE libvirt_dom_open_console(int argc, VALUE *argv, VALUE d)
{
    VALUE dev, st, flags;

    rb_scan_args(argc, argv, "21", &dev, &st, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainOpenConsole, connect_get(d), domain_get(d),
                  StringValueCStr(dev), stream_get(st), NUM2INT(flags));
}
#endif

#if HAVE_VIRDOMAINSCREENSHOT
/*
 * call-seq:
 *   dom.screenshot(stream, screen, flags=0) -> nil
 *
 * Call virDomainScreenshot[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainScreenshot]
 * to take a screenshot of the domain console as a stream.
 */
static VALUE libvirt_dom_screenshot(int argc, VALUE *argv, VALUE d)
{
    VALUE st, screen, flags;

    rb_scan_args(argc, argv, "21", &st, &screen, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_string(virDomainScreenshot, connect_get(d), 1, domain_get(d),
                    stream_get(st), NUM2UINT(screen), NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAININJECTNMI
/*
 * call-seq:
 *   dom.inject_nmi(flags=0) -> nil
 *
 * Call virDomainInjectNMI[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainInjectNMI]
 * to send an NMI to the guest.
 */
static VALUE libvirt_dom_inject_nmi(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainInjectNMI, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINGETCONTROLINFO
/*
 * call-seq:
 *   dom.control_info(flags=0) -> Libvirt::Domain::ControlInfo
 *
 * Call virDomainGetControlInfo[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetControlInfo]
 * to retrieve domain control interface information.
 */
static VALUE libvirt_dom_control_info(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;
    virDomainControlInfo info;
    int r;
    VALUE result;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    r = virDomainGetControlInfo(domain_get(d), &info, NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainGetControlInfo",
                           connect_get(d)));

    result = rb_class_new_instance(0, NULL, c_domain_control_info);
    rb_iv_set(result, "@state", ULONG2NUM(info.state));
    rb_iv_set(result, "@details", ULONG2NUM(info.details));
    rb_iv_set(result, "@stateTime", ULL2NUM(info.stateTime));

    return result;
}
#endif

#if HAVE_VIRDOMAINSENDKEY
/*
 * call-seq:
 *   dom.send_key(codeset, holdtime, keycodes)
 *
 * Call virDomainSendKey[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSendKey]
 * to send key(s) to the domain. Keycodes has to be an array of keys to send.
 */
VALUE libvirt_dom_send_key(VALUE d, VALUE codeset, VALUE holdtime,
                           VALUE keycodes)
{
    unsigned int codes[RARRAY_LEN(keycodes)];
    int i = 0;

    for (i = 0; i < RARRAY_LEN(keycodes); i++) {
        codes[i] = NUM2UINT(rb_ary_entry(keycodes,i));
    }

    gen_call_void(virDomainSendKey, connect_get(d), domain_get(d),
                  NUM2UINT(codeset), NUM2UINT(holdtime), codes,
                  RARRAY_LEN(keycodes), 0);
}
#endif

#if HAVE_VIRDOMAINMIGRATEGETMAXSPEED
/*
 * call-seq:
 *   dom.migrate_max_speed(flags=0) -> fixnum
 *
 * Call virDomainMigrateGetMaxSpeed[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainMigrateGetMaxSpeed]
 * to retrieve the maximum speed a migration can use.
 */
static VALUE libvirt_dom_migrate_max_speed(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;
    int r;
    unsigned long bandwidth;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    r = virDomainMigrateGetMaxSpeed(domain_get(d), &bandwidth, NUM2UINT(flags));
    _E(r < 0, create_error(e_RetrieveError, "virDomainMigrateGetMaxSpeed",
                           connect_get(d)));

    return ULONG2NUM(bandwidth);
}
#endif

#if HAVE_VIRDOMAINRESET
/*
 * call-seq:
 *   dom.reset(flags=0) -> nil
 *
 * Call virDomainReset[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainReset]
 * to reset a domain immediately.
 */
static VALUE libvirt_dom_reset(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainReset, connect_get(d), domain_get(d),
                  NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINGETHOSTNAME
/*
 * call-seq:
 *   dom.hostname(flags=0) -> nil
 *
 * Call virDomainGetHostname[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetHostname]
 * to get the hostname from a domain.
 */
static VALUE libvirt_dom_hostname(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_string(virDomainGetHostname, connect_get(d), 1, domain_get(d),
                    NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINGETMETADATA
/*
 * call-seq:
 *   dom.metadata(flags=0) -> string
 *
 * Call virDomainGetMetadata[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainGetMetadata]
 * to get the metadata from a domain.
 */
static VALUE libvirt_dom_metadata(int argc, VALUE *argv, VALUE d)
{
    VALUE uri, flags, type;

    rb_scan_args(argc, argv, "12", &type, &uri, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_string(virDomainGetMetadata, connect_get(d), 1, domain_get(d),
                    NUM2INT(type), get_string_or_nil(uri), NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINSETMETADATA
/*
 * call-seq:
 *   dom.metadata = fixnum,string/nil,key=nil,uri=nil,flags=0 -> nil
 *
 * Call virDomainSetMetadata[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSetMetadata]
 * to set the metadata for a domain.
 */
static VALUE libvirt_dom_set_metadata(VALUE d, VALUE in)
{
    VALUE type, metadata, key, uri, flags;

    if (TYPE(in) != T_ARRAY) {
        rb_raise(rb_eTypeError,
                 "wrong argument type (expected Array)");
    }

    if (RARRAY_LEN(in) < 2 || RARRAY_LEN(in) > 5) {
        rb_raise(rb_eArgError,
                 "wrong number of arguments (%ld for 2, 3, 4, or 5)",
                 RARRAY_LEN(in));
    }

    type = rb_ary_entry(in, 0);
    metadata = rb_ary_entry(in, 1);
    key = Qnil;
    uri = Qnil;
    flags = INT2NUM(0);

    if (RARRAY_LEN(in) >= 3) {
        key = rb_ary_entry(in, 2);
    }
    if (RARRAY_LEN(in) >= 4) {
        uri = rb_ary_entry(in, 3);
    }
    if (RARRAY_LEN(in) == 5) {
        flags = rb_ary_entry(in, 4);
    }

    gen_call_void(virDomainSetMetadata, connect_get(d), domain_get(d),
                  NUM2INT(type), get_string_or_nil(metadata),
                  get_string_or_nil(key), get_string_or_nil(uri),
                  NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINSENDPROCESSSIGNAL
/*
 * call-seq:
 *   dom.send_process_signal(pid, signum, flags=0) -> nil
 *
 * Call virDomainSendProcessSignal[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSendProcessSignal]
 * to send a signal to a process inside the domain.
 */
static VALUE libvirt_dom_send_process_signal(int argc, VALUE *argv, VALUE d)
{
    VALUE pid, signum, flags;

    rb_scan_args(argc, argv, "21", &pid, &signum, &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_void(virDomainSendProcessSignal, connect_get(d), domain_get(d),
                  NUM2LL(pid), NUM2UINT(signum), NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINLISTALLSNAPSHOTS
/*
 * call-seq:
 *   dom.list_all_snapshots(flags=0) -> array
 *
 * Call virDomainListAllSnapshots[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainListAllSnapshots]
 * to get an array of snapshot objects for all snapshots.
 */
static VALUE libvirt_dom_list_all_snapshots(int argc, VALUE *argv, VALUE d)
{
    gen_list_all(virDomainSnapshotPtr, argc, argv, virDomainListAllSnapshots,
                 domain_get(d), d, domain_snapshot_new, virDomainSnapshotFree);
}
#endif

#if HAVE_VIRDOMAINSNAPSHOTNUMCHILDREN
/*
 * call-seq:
 *   snapshot.num_children(flags=0) -> fixnum
 *
 * Call virDomainSnapshotNumChildren[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotNumChildren]
 * to get the number of children snapshots of this snapshot.
 */
static VALUE libvirt_dom_snapshot_num_children(int argc, VALUE *argv, VALUE d)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_int(virDomainSnapshotNumChildren, connect_get(d),
                 domain_snapshot_get(d), NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINSNAPSHOTLISTCHILDRENNAMES
/*
 * call-seq:
 *   snapshot.list_children_names(flags=0) -> array
 *
 * Call virDomainSnapshotListChildrenNames[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotListChildrenNames]
 * to get an array of strings representing the children of this snapshot.
 */
static VALUE libvirt_dom_snapshot_list_children_names(int argc, VALUE *argv,
                                                      VALUE d)
{
    VALUE flags;
    int num_children;
    char **children;
    int ret;
    int i, j;
    VALUE result;
    VALUE str;
    int exception = 0;
    struct rb_ary_store_wrap arg;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    num_children = virDomainSnapshotNumChildren(domain_snapshot_get(d),
                                                NUM2UINT(flags));
    _E(num_children < 0, create_error(e_RetrieveError,
                                      "virDomainSnapshotNumChildren",
                                      connect_get(d)));

    children = alloca(num_children * sizeof(char *));

    result = rb_ary_new();

    ret = virDomainSnapshotListChildrenNames(domain_snapshot_get(d), children,
                                             num_children, NUM2UINT(flags));
    _E(ret < 0, create_error(e_RetrieveError,
                             "virDomainSnapshotListChildrenNames",
                             connect_get(d)));

    for (i = 0; i < ret; i++) {
        str = rb_protect(rb_str_new2_wrap, (VALUE)&(children[i]), &exception);
        if (exception) {
            goto error;
        }

        arg.arr = result;
        arg.index = INT2NUM(i);
        arg.elem = str;
        rb_protect(rb_ary_store_wrap, (VALUE)&arg, &exception);
        if (exception) {
            goto error;
        }
        free(children[i]);
    }

    return result;

error:
    for (j = i; j < ret; j++) {
        free(children[j]);
    }
    rb_jump_tag(exception);

    /* not necessary, just to shut the compiler up */
    return Qnil;
}
#endif

#if HAVE_VIRDOMAINSNAPSHOTLISTALLCHILDREN
/*
 * call-seq:
 *   snapshot.list_all_children(flags=0) -> array
 *
 * Call virDomainSnapshotListAllChildren[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotListAllChildren]
 * to get an array of snapshot objects that are children of this snapshot.
 */
static VALUE libvirt_dom_snapshot_list_all_children(int argc, VALUE *argv,
                                                    VALUE s)
{
    gen_list_all(virDomainSnapshotPtr, argc, argv,
                 virDomainSnapshotListAllChildren, domain_snapshot_get(s), s,
                 domain_snapshot_new, virDomainSnapshotFree);
}
#endif

#if HAVE_VIRDOMAINSNAPSHOTGETPARENT
/*
 * call-seq:
 *   snapshot.parent(flags=0) -> [Libvirt::Domain::Snapshot|nil]
 *
 * Call virDomainSnapshotGetParent[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotGetParent]
 * to get the parent of this snapshot (nil will be returned if this is a root
 * snapshot).
 */
static VALUE libvirt_dom_snapshot_parent(int argc, VALUE *argv, VALUE s)
{
    virDomainSnapshotPtr snap;
    VALUE flags;
    virErrorPtr err;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    snap = virDomainSnapshotGetParent(domain_snapshot_get(s), NUM2UINT(flags));
    if (snap == NULL) {
        /* snap may be NULL if there is a root, in which case we want to return
         * nil
         */
        err = virConnGetLastError(connect_get(s));
        if (err->code == VIR_ERR_NO_DOMAIN_SNAPSHOT) {
            return Qnil;
        }

        rb_exc_raise(create_error(e_RetrieveError, "virDomainSnapshotGetParent",
                                  connect_get(s)));
    }

    return domain_snapshot_new(snap, s);
}
#endif

#if HAVE_VIRDOMAINSNAPSHOTISCURRENT
/*
 * call-seq:
 *   snapshot.current?(flags=0) -> [true|false]
 *
 * Call virDomainSnapshotIsCurrent[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotIsCurrent]
 * to determine if the snapshot is the domain's current snapshot.
 */
static VALUE libvirt_domain_snapshot_current_p(int argc, VALUE *argv, VALUE s)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_truefalse(virDomainSnapshotIsCurrent, connect_get(s),
                       domain_snapshot_get(s), NUM2UINT(flags));
}
#endif

#if HAVE_VIRDOMAINSNAPSHOTHASMETADATA
/*
 * call-seq:
 *   snapshot.has_metadata?(flags=0) -> [true|false]
 *
 * Call virDomainSnapshotHasMetadata[http://www.libvirt.org/html/libvirt-libvirt.html#virDomainSnapshotHasMetadata]
 * to determine if the snapshot is associated with libvirt metadata.
 */
static VALUE libvirt_domain_snapshot_has_metadata_p(int argc, VALUE *argv,
                                                    VALUE s)
{
    VALUE flags;

    rb_scan_args(argc, argv, "01", &flags);

    flags = integer_default_if_nil(flags, 0);

    gen_call_truefalse(virDomainSnapshotHasMetadata, connect_get(s),
                       domain_snapshot_get(s), NUM2UINT(flags));
}
#endif

/*
 * Class Libvirt::Domain
 */
void init_domain()
{
    c_domain = rb_define_class_under(m_libvirt, "Domain", rb_cObject);

    rb_define_const(c_domain, "NOSTATE", INT2NUM(VIR_DOMAIN_NOSTATE));
    rb_define_const(c_domain, "RUNNING", INT2NUM(VIR_DOMAIN_RUNNING));
    rb_define_const(c_domain, "BLOCKED", INT2NUM(VIR_DOMAIN_BLOCKED));
    rb_define_const(c_domain, "PAUSED", INT2NUM(VIR_DOMAIN_PAUSED));
    rb_define_const(c_domain, "SHUTDOWN", INT2NUM(VIR_DOMAIN_SHUTDOWN));
    rb_define_const(c_domain, "SHUTOFF", INT2NUM(VIR_DOMAIN_SHUTOFF));
    rb_define_const(c_domain, "CRASHED", INT2NUM(VIR_DOMAIN_CRASHED));
#if HAVE_CONST_VIR_DOMAIN_PMSUSPENDED
    rb_define_const(c_domain, "PMSUSPENDED", INT2NUM(VIR_DOMAIN_PMSUSPENDED));
#endif

    /* virDomainMigrateFlags */
#if HAVE_CONST_VIR_MIGRATE_LIVE
    rb_define_const(c_domain, "MIGRATE_LIVE", INT2NUM(VIR_MIGRATE_LIVE));
#endif
#if HAVE_CONST_VIR_MIGRATE_PEER2PEER
    rb_define_const(c_domain, "MIGRATE_PEER2PEER",
                    INT2NUM(VIR_MIGRATE_PEER2PEER));
#endif
#if HAVE_CONST_VIR_MIGRATE_TUNNELLED
    rb_define_const(c_domain, "MIGRATE_TUNNELLED",
                    INT2NUM(VIR_MIGRATE_TUNNELLED));
#endif
#if HAVE_CONST_VIR_MIGRATE_PERSIST_DEST
    rb_define_const(c_domain, "MIGRATE_PERSIST_DEST",
                    INT2NUM(VIR_MIGRATE_PERSIST_DEST));
#endif
#if HAVE_CONST_VIR_MIGRATE_UNDEFINE_SOURCE
    rb_define_const(c_domain, "MIGRATE_UNDEFINE_SOURCE",
                    INT2NUM(VIR_MIGRATE_UNDEFINE_SOURCE));
#endif
#if HAVE_CONST_VIR_MIGRATE_PAUSED
    rb_define_const(c_domain, "MIGRATE_PAUSED", INT2NUM(VIR_MIGRATE_PAUSED));
#endif
#if HAVE_CONST_VIR_MIGRATE_NON_SHARED_DISK
    rb_define_const(c_domain, "MIGRATE_NON_SHARED_DISK",
                    INT2NUM(VIR_MIGRATE_NON_SHARED_DISK));
#endif
#if HAVE_CONST_VIR_MIGRATE_NON_SHARED_INC
    rb_define_const(c_domain, "MIGRATE_NON_SHARED_INC",
                    INT2NUM(VIR_MIGRATE_NON_SHARED_INC));
#endif
#if HAVE_CONST_VIR_MIGRATE_CHANGE_PROTECTION
    rb_define_const(c_domain, "MIGRATE_CHANGE_PROTECTION",
                    INT2NUM(VIR_MIGRATE_CHANGE_PROTECTION));
#endif
    rb_define_const(c_domain, "DOMAIN_XML_SECURE",
                    INT2NUM(VIR_DOMAIN_XML_SECURE));
    rb_define_const(c_domain, "DOMAIN_XML_INACTIVE",
                    INT2NUM(VIR_DOMAIN_XML_INACTIVE));
#if HAVE_CONST_VIR_DOMAIN_XML_UPDATE_CPU
    rb_define_const(c_domain, "DOMAIN_XML_UPDATE_CPU",
                    INT2NUM(VIR_DOMAIN_XML_UPDATE_CPU));
#endif
#if HAVE_VIRDOMAINMEMORYPEEK
    rb_define_const(c_domain, "MEMORY_VIRTUAL", INT2NUM(VIR_MEMORY_VIRTUAL));
#endif
#if HAVE_CONST_VIR_MEMORY_PHYSICAL
    rb_define_const(c_domain, "MEMORY_PHYSICAL", INT2NUM(VIR_MEMORY_PHYSICAL));
#endif

#if HAVE_CONST_VIR_DOMAIN_START_PAUSED
    rb_define_const(c_domain, "START_PAUSED", INT2NUM(VIR_DOMAIN_START_PAUSED));
#endif

#if HAVE_CONST_VIR_DOMAIN_START_AUTODESTROY
    rb_define_const(c_domain, "START_AUTODESTROY",
                    INT2NUM(VIR_DOMAIN_START_AUTODESTROY));
#endif

#if HAVE_CONST_VIR_DOMAIN_START_BYPASS_CACHE
    rb_define_const(c_domain, "START_BYPASS_CACHE",
                    INT2NUM(VIR_DOMAIN_START_BYPASS_CACHE));
#endif

#if HAVE_CONST_VIR_DOMAIN_START_FORCE_BOOT
    rb_define_const(c_domain, "START_FORCE_BOOT",
                    INT2NUM(VIR_DOMAIN_START_FORCE_BOOT));
#endif

#if HAVE_CONST_VIR_DUMP_CRASH
    rb_define_const(c_domain, "DUMP_CRASH", INT2NUM(VIR_DUMP_CRASH));
#endif
#if HAVE_CONST_VIR_DUMP_LIVE
    rb_define_const(c_domain, "DUMP_LIVE", INT2NUM(VIR_DUMP_LIVE));
#endif
#if HAVE_CONST_VIR_DUMP_BYPASS_CACHE
    rb_define_const(c_domain, "BYPASS_CACHE", INT2NUM(VIR_DUMP_BYPASS_CACHE));
#endif
#if HAVE_CONST_VIR_DUMP_RESET
    rb_define_const(c_domain, "RESET", INT2NUM(VIR_DUMP_RESET));
#endif
#if HAVE_CONST_VIR_DUMP_MEMORY_ONLY
    rb_define_const(c_domain, "MEMORY_ONLY", INT2NUM(VIR_DUMP_MEMORY_ONLY));
#endif

#if HAVE_VIRDOMAINGETVCPUSFLAGS
    rb_define_const(c_domain, "VCPU_LIVE", INT2NUM(VIR_DOMAIN_VCPU_LIVE));
    rb_define_const(c_domain, "VCPU_CONFIG", INT2NUM(VIR_DOMAIN_VCPU_CONFIG));
    rb_define_const(c_domain, "VCPU_MAXIMUM", INT2NUM(VIR_DOMAIN_VCPU_MAXIMUM));
#endif

    rb_define_method(c_domain, "migrate", libvirt_dom_migrate, -1);
#if HAVE_VIRDOMAINMIGRATETOURI
    rb_define_method(c_domain, "migrate_to_uri",
                     libvirt_dom_migrate_to_uri, -1);
#endif
#if HAVE_VIRDOMAINMIGRATESETMAXDOWNTIME
    rb_define_method(c_domain, "migrate_set_max_downtime",
                     libvirt_dom_migrate_set_max_downtime, -1);
#endif
#if HAVE_VIRDOMAINMIGRATE2
    rb_define_method(c_domain, "migrate2", libvirt_dom_migrate2, -1);
    rb_define_method(c_domain, "migrate_to_uri2",
                     libvirt_dom_migrate_to_uri2, -1);
    rb_define_method(c_domain, "migrate_set_max_speed",
                     libvirt_dom_migrate_set_max_speed, -1);
#endif

#if HAVE_CONST_VIR_DOMAIN_SAVE_BYPASS_CACHE
    rb_define_const(c_domain, "SAVE_BYPASS_CACHE",
                    INT2NUM(VIR_DOMAIN_SAVE_BYPASS_CACHE));
#endif
#if HAVE_CONST_VIR_DOMAIN_SAVE_RUNNING
    rb_define_const(c_domain, "SAVE_RUNNING", INT2NUM(VIR_DOMAIN_SAVE_RUNNING));
#endif
#if HAVE_CONST_VIR_DOMAIN_SAVE_PAUSED
    rb_define_const(c_domain, "SAVE_PAUSED", INT2NUM(VIR_DOMAIN_SAVE_PAUSED));
#endif

#if HAVE_CONST_VIR_DOMAIN_UNDEFINE_MANAGED_SAVE
    rb_define_const(c_domain, "UNDEFINE_MANAGED_SAVE",
                    INT2NUM(VIR_DOMAIN_UNDEFINE_MANAGED_SAVE));
#endif
#if HAVE_CONST_VIR_DOMAIN_UNDEFINE_SNAPSHOTS_METADATA
    rb_define_const(c_domain, "UNDEFINE_SNAPSHOTS_METADATA",
                    INT2NUM(VIR_DOMAIN_UNDEFINE_SNAPSHOTS_METADATA));
#endif

    rb_define_attr(c_domain, "connection", 1, 0);

#if HAVE_CONST_VIR_DOMAIN_SHUTDOWN_DEFAULT
    rb_define_const(c_domain, "SHUTDOWN_DEFAULT",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_DEFAULT));
#endif
#if HAVE_CONST_VIR_DOMAIN_SHUTDOWN_ACPI_POWER_BTN
    rb_define_const(c_domain, "SHUTDOWN_ACPI_POWER_BTN",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_ACPI_POWER_BTN));
#endif
#if HAVE_CONST_VIR_DOMAIN_SHUTDOWN_GUEST_AGENT
    rb_define_const(c_domain, "SHUTDOWN_GUEST_AGENT",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_GUEST_AGENT));
#endif
#if HAVE_CONST_VIR_DOMAIN_SHUTDOWN_INITCTL
    rb_define_const(c_domain, "SHUTDOWN_INITCTL",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_INITCTL));
#endif
#if HAVE_CONST_VIR_DOMAIN_SHUTDOWN_SIGNAL
    rb_define_const(c_domain, "SHUTDOWN_SIGNAL",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_SIGNAL));
#endif
    rb_define_method(c_domain, "shutdown", libvirt_dom_shutdown, -1);

#if HAVE_CONST_VIR_DOMAIN_REBOOT_DEFAULT
    rb_define_const(c_domain, "REBOOT_DEFAULT",
                    INT2NUM(VIR_DOMAIN_REBOOT_DEFAULT));
#endif
#if HAVE_CONST_VIR_DOMAIN_REBOOT_ACPI_POWER_BTN
    rb_define_const(c_domain, "REBOOT_ACPI_POWER_BTN",
                    INT2NUM(VIR_DOMAIN_REBOOT_ACPI_POWER_BTN));
#endif
#if HAVE_CONST_VIR_DOMAIN_REBOOT_GUEST_AGENT
    rb_define_const(c_domain, "REBOOT_GUEST_AGENT",
                    INT2NUM(VIR_DOMAIN_REBOOT_GUEST_AGENT));
#endif
#if HAVE_CONST_VIR_DOMAIN_REBOOT_INITCTL
    rb_define_const(c_domain, "REBOOT_INITCTL",
                    INT2NUM(VIR_DOMAIN_REBOOT_INITCTL));
#endif
#if HAVE_CONST_VIR_DOMAIN_REBOOT_SIGNAL
    rb_define_const(c_domain, "REBOOT_SIGNAL",
                    INT2NUM(VIR_DOMAIN_REBOOT_SIGNAL));
#endif
    rb_define_method(c_domain, "reboot", libvirt_dom_reboot, -1);
#if HAVE_CONST_VIR_DOMAIN_DESTROY_DEFAULT
    rb_define_const(c_domain, "DESTROY_DEFAULT",
                    INT2NUM(VIR_DOMAIN_DESTROY_DEFAULT));
#endif
#if HAVE_CONST_VIR_DOMAIN_DESTROY_GRACEFUL
    rb_define_const(c_domain, "DESTROY_GRACEFUL",
                    INT2NUM(VIR_DOMAIN_DESTROY_GRACEFUL));
#endif
    rb_define_method(c_domain, "destroy", libvirt_dom_destroy, -1);
    rb_define_method(c_domain, "suspend", libvirt_dom_suspend, 0);
    rb_define_method(c_domain, "resume", libvirt_dom_resume, 0);
    rb_define_method(c_domain, "save", libvirt_dom_save, -1);
    rb_define_singleton_method(c_domain, "restore", libvirt_dom_s_restore, 2);
    rb_define_method(c_domain, "core_dump", libvirt_dom_core_dump, -1);
    rb_define_method(c_domain, "info", libvirt_dom_info, 0);
    rb_define_method(c_domain, "ifinfo", libvirt_dom_if_stats, 1);
    rb_define_method(c_domain, "name", libvirt_dom_name, 0);
    rb_define_method(c_domain, "id", libvirt_dom_id, 0);
    rb_define_method(c_domain, "uuid", libvirt_dom_uuid, 0);
    rb_define_method(c_domain, "os_type", libvirt_dom_os_type, 0);
    rb_define_method(c_domain, "max_memory", libvirt_dom_max_memory, 0);
    rb_define_method(c_domain, "max_memory=", libvirt_dom_max_memory_set, 1);
    rb_define_method(c_domain, "memory=", libvirt_dom_memory_set, 1);
    rb_define_method(c_domain, "max_vcpus", libvirt_dom_max_vcpus, 0);
    rb_define_method(c_domain, "vcpus=", libvirt_dom_vcpus_set, 1);
#if HAVE_VIRDOMAINSETVCPUSFLAGS
    rb_define_method(c_domain, "vcpus_flags=", libvirt_dom_vcpus_set_flags, 1);
#endif
    rb_define_method(c_domain, "pin_vcpu", libvirt_dom_pin_vcpu, -1);
    rb_define_method(c_domain, "xml_desc", libvirt_dom_xml_desc, -1);
    rb_define_method(c_domain, "undefine", libvirt_dom_undefine, -1);
    rb_define_method(c_domain, "create", libvirt_dom_create, -1);
    rb_define_method(c_domain, "autostart", libvirt_dom_autostart, 0);
    rb_define_method(c_domain, "autostart?", libvirt_dom_autostart, 0);
    rb_define_method(c_domain, "autostart=", libvirt_dom_autostart_set, 1);
    rb_define_method(c_domain, "free", libvirt_dom_free, 0);

#if HAVE_CONST_VIR_DOMAIN_DEVICE_MODIFY_CURRENT
    rb_define_const(c_domain, "DEVICE_MODIFY_CURRENT",
                    INT2NUM(VIR_DOMAIN_DEVICE_MODIFY_CURRENT));
#endif
#if HAVE_CONST_VIR_DOMAIN_DEVICE_MODIFY_LIVE
    rb_define_const(c_domain, "DEVICE_MODIFY_LIVE",
                    INT2NUM(VIR_DOMAIN_DEVICE_MODIFY_LIVE));
#endif
#if HAVE_CONST_VIR_DOMAIN_DEVICE_MODIFY_CONFIG
    rb_define_const(c_domain, "DEVICE_MODIFY_CONFIG",
                    INT2NUM(VIR_DOMAIN_DEVICE_MODIFY_CONFIG));
#endif
#if HAVE_CONST_VIR_DOMAIN_DEVICE_MODIFY_FORCE
    rb_define_const(c_domain, "DEVICE_MODIFY_FORCE",
                    INT2NUM(VIR_DOMAIN_DEVICE_MODIFY_FORCE));
#endif
    rb_define_method(c_domain, "attach_device", libvirt_dom_attach_device, -1);
    rb_define_method(c_domain, "detach_device", libvirt_dom_detach_device, -1);
#if HAVE_VIRDOMAINUPDATEDEVICEFLAGS
    rb_define_method(c_domain, "update_device", libvirt_dom_update_device, -1);
#endif

    rb_define_method(c_domain, "scheduler_type", libvirt_dom_scheduler_type, 0);

#if HAVE_VIRDOMAINMANAGEDSAVE
    rb_define_method(c_domain, "managed_save", libvirt_dom_managed_save, -1);
    rb_define_method(c_domain, "has_managed_save?",
                     libvirt_dom_has_managed_save, -1);
    rb_define_method(c_domain, "managed_save_remove",
                     libvirt_dom_managed_save_remove, -1);
#endif
#if HAVE_VIRDOMAINGETSECURITYLABEL
    rb_define_method(c_domain, "security_label",
                     libvirt_dom_security_label, 0);
#endif
    rb_define_method(c_domain, "block_stats", libvirt_dom_block_stats, 1);
#if HAVE_TYPE_VIRDOMAINMEMORYSTATPTR
    rb_define_method(c_domain, "memory_stats", libvirt_dom_memory_stats, -1);
#endif
#if HAVE_VIRDOMAINBLOCKPEEK
    rb_define_method(c_domain, "block_peek", libvirt_dom_block_peek, -1);
#endif
#if HAVE_TYPE_VIRDOMAINBLOCKINFOPTR
    rb_define_method(c_domain, "blockinfo", libvirt_dom_block_info, -1);
#endif
#if HAVE_VIRDOMAINMEMORYPEEK
    rb_define_method(c_domain, "memory_peek", libvirt_dom_memory_peek, -1);
#endif
    rb_define_method(c_domain, "get_vcpus", libvirt_dom_get_vcpus, 0);
#if HAVE_VIRDOMAINISACTIVE
    rb_define_method(c_domain, "active?", libvirt_dom_active_p, 0);
#endif
#if HAVE_VIRDOMAINISPERSISTENT
    rb_define_method(c_domain, "persistent?", libvirt_dom_persistent_p, 0);
#endif
#if HAVE_TYPE_VIRDOMAINSNAPSHOTPTR
    rb_define_method(c_domain, "snapshot_create_xml",
                     libvirt_dom_snapshot_create_xml, -1);
    rb_define_method(c_domain, "num_of_snapshots",
                     libvirt_dom_num_of_snapshots, -1);
    rb_define_method(c_domain, "list_snapshots",
                     libvirt_dom_list_snapshots, -1);
    rb_define_method(c_domain, "lookup_snapshot_by_name",
                     libvirt_dom_lookup_snapshot_by_name, -1);
    rb_define_method(c_domain, "has_current_snapshot?",
                     libvirt_dom_has_current_snapshot_p, -1);
    rb_define_method(c_domain, "revert_to_snapshot",
                     libvirt_dom_revert_to_snapshot, -1);
    rb_define_method(c_domain, "current_snapshot",
                     libvirt_dom_current_snapshot, -1);
#endif

    /*
     * Class Libvirt::Domain::Info
     */
    c_domain_info = rb_define_class_under(c_domain, "Info", rb_cObject);
    rb_define_attr(c_domain_info, "state", 1, 0);
    rb_define_attr(c_domain_info, "max_mem", 1, 0);
    rb_define_attr(c_domain_info, "memory", 1, 0);
    rb_define_attr(c_domain_info, "nr_virt_cpu", 1, 0);
    rb_define_attr(c_domain_info, "cpu_time", 1, 0);

    /*
     * Class Libvirt::Domain::InterfaceInfo
     */
    c_domain_ifinfo = rb_define_class_under(c_domain, "InterfaceInfo",
                                            rb_cObject);
    rb_define_attr(c_domain_ifinfo, "rx_bytes", 1, 0);
    rb_define_attr(c_domain_ifinfo, "rx_packets", 1, 0);
    rb_define_attr(c_domain_ifinfo, "rx_errs", 1, 0);
    rb_define_attr(c_domain_ifinfo, "rx_drop", 1, 0);
    rb_define_attr(c_domain_ifinfo, "tx_bytes", 1, 0);
    rb_define_attr(c_domain_ifinfo, "tx_packets", 1, 0);
    rb_define_attr(c_domain_ifinfo, "tx_errs", 1, 0);
    rb_define_attr(c_domain_ifinfo, "tx_drop", 1, 0);

    /*
     * Class Libvirt::Domain::SecurityLabel
     */
    c_domain_security_label = rb_define_class_under(c_domain, "SecurityLabel",
                                                    rb_cObject);
    rb_define_attr(c_domain_security_label, "label", 1, 0);
    rb_define_attr(c_domain_security_label, "enforcing", 1, 0);

    /*
     * Class Libvirt::Domain::BlockStats
     */
    c_domain_block_stats = rb_define_class_under(c_domain, "BlockStats",
                                                 rb_cObject);
    rb_define_attr(c_domain_block_stats, "rd_req", 1, 0);
    rb_define_attr(c_domain_block_stats, "rd_bytes", 1, 0);
    rb_define_attr(c_domain_block_stats, "wr_req", 1, 0);
    rb_define_attr(c_domain_block_stats, "wr_bytes", 1, 0);
    rb_define_attr(c_domain_block_stats, "errs", 1, 0);

#if HAVE_TYPE_VIRDOMAINMEMORYSTATPTR
    /*
     * Class Libvirt::Domain::MemoryStats
     */
    c_domain_memory_stats = rb_define_class_under(c_domain, "MemoryStats",
                                                  rb_cObject);
    rb_define_attr(c_domain_memory_stats, "tag", 1, 0);
    rb_define_attr(c_domain_memory_stats, "value", 1, 0);

    rb_define_const(c_domain_memory_stats, "SWAP_IN",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_SWAP_IN));
    rb_define_const(c_domain_memory_stats, "SWAP_OUT",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_SWAP_OUT));
    rb_define_const(c_domain_memory_stats, "MAJOR_FAULT",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_MAJOR_FAULT));
    rb_define_const(c_domain_memory_stats, "MINOR_FAULT",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_MINOR_FAULT));
    rb_define_const(c_domain_memory_stats, "UNUSED",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_UNUSED));
    rb_define_const(c_domain_memory_stats, "AVAILABLE",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_AVAILABLE));
#if HAVE_CONST_VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON
    rb_define_const(c_domain_memory_stats, "ACTUAL_BALLOON",
                    INT2NUM(VIR_DOMAIN_MEMORY_STAT_ACTUAL_BALLOON));
#endif
#endif

#if HAVE_TYPE_VIRDOMAINBLOCKINFOPTR
    /*
     * Class Libvirt::Domain::BlockInfo
     */
    c_domain_block_info = rb_define_class_under(c_domain, "BlockInfo",
                                                rb_cObject);
    rb_define_attr(c_domain_block_info, "capacity", 1, 0);
    rb_define_attr(c_domain_block_info, "allocation", 1, 0);
    rb_define_attr(c_domain_block_info, "physical", 1, 0);
#endif

#if HAVE_TYPE_VIRDOMAINSNAPSHOTPTR
    /*
     * Class Libvirt::Domain::Snapshot
     */
    c_domain_snapshot = rb_define_class_under(c_domain, "Snapshot", rb_cObject);
    rb_define_const(c_domain_snapshot, "DELETE_CHILDREN",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_DELETE_CHILDREN));
    rb_define_method(c_domain_snapshot, "xml_desc",
                     libvirt_dom_snapshot_xml_desc, -1);
    rb_define_method(c_domain_snapshot, "delete",
                     libvirt_dom_snapshot_delete, -1);
    rb_define_method(c_domain_snapshot, "free", libvirt_dom_snapshot_free, 0);
#endif
#if HAVE_VIRDOMAINSNAPSHOTGETNAME
    rb_define_method(c_domain_snapshot, "name", libvirt_dom_snapshot_name, 0);
#endif

    /*
     * Class Libvirt::Domain::VCPUInfo
     */
    c_domain_vcpuinfo = rb_define_class_under(c_domain, "VCPUInfo", rb_cObject);
    rb_define_const(c_domain_vcpuinfo, "OFFLINE", VIR_VCPU_OFFLINE);
    rb_define_const(c_domain_vcpuinfo, "RUNNING", VIR_VCPU_RUNNING);
    rb_define_const(c_domain_vcpuinfo, "BLOCKED", VIR_VCPU_BLOCKED);
    rb_define_attr(c_domain_vcpuinfo, "number", 1, 0);
    rb_define_attr(c_domain_vcpuinfo, "state", 1, 0);
    rb_define_attr(c_domain_vcpuinfo, "cpu_time", 1, 0);
    rb_define_attr(c_domain_vcpuinfo, "cpu", 1, 0);
    rb_define_attr(c_domain_vcpuinfo, "cpumap", 1, 0);

#if HAVE_TYPE_VIRDOMAINJOBINFOPTR
    /*
     * Class Libvirt::Domain::JobInfo
     */
    c_domain_job_info = rb_define_class_under(c_domain, "JobInfo", rb_cObject);
    rb_define_const(c_domain_job_info, "NONE", INT2NUM(VIR_DOMAIN_JOB_NONE));
    rb_define_const(c_domain_job_info, "BOUNDED",
                    INT2NUM(VIR_DOMAIN_JOB_BOUNDED));
    rb_define_const(c_domain_job_info, "UNBOUNDED",
                    INT2NUM(VIR_DOMAIN_JOB_UNBOUNDED));
    rb_define_const(c_domain_job_info, "COMPLETED",
                    INT2NUM(VIR_DOMAIN_JOB_COMPLETED));
    rb_define_const(c_domain_job_info, "FAILED",
                    INT2NUM(VIR_DOMAIN_JOB_FAILED));
    rb_define_const(c_domain_job_info, "CANCELLED",
                    INT2NUM(VIR_DOMAIN_JOB_CANCELLED));
    rb_define_attr(c_domain_job_info, "type", 1, 0);
    rb_define_attr(c_domain_job_info, "time_elapsed", 1, 0);
    rb_define_attr(c_domain_job_info, "time_remaining", 1, 0);
    rb_define_attr(c_domain_job_info, "data_total", 1, 0);
    rb_define_attr(c_domain_job_info, "data_processed", 1, 0);
    rb_define_attr(c_domain_job_info, "data_remaining", 1, 0);
    rb_define_attr(c_domain_job_info, "mem_total", 1, 0);
    rb_define_attr(c_domain_job_info, "mem_processed", 1, 0);
    rb_define_attr(c_domain_job_info, "mem_remaining", 1, 0);
    rb_define_attr(c_domain_job_info, "file_total", 1, 0);
    rb_define_attr(c_domain_job_info, "file_processed", 1, 0);
    rb_define_attr(c_domain_job_info, "file_remaining", 1, 0);

    rb_define_method(c_domain, "job_info", libvirt_dom_job_info, 0);
    rb_define_method(c_domain, "abort_job", libvirt_dom_abort_job, 0);
#endif

#if HAVE_VIRDOMAINQEMUMONITORCOMMAND
    rb_define_method(c_domain, "qemu_monitor_command",
                     libvirt_dom_qemu_monitor_command, -1);
#endif

#if HAVE_VIRDOMAINGETVCPUSFLAGS
    rb_define_method(c_domain, "num_vcpus", libvirt_dom_num_vcpus, 1);
#endif

#if HAVE_VIRDOMAINISUPDATED
    rb_define_method(c_domain, "updated?", libvirt_dom_is_updated, 0);
#endif

#ifdef VIR_DOMAIN_MEMORY_PARAM_UNLIMITED
    rb_define_const(c_domain, "MEMORY_PARAM_UNLIMITED",
                    VIR_DOMAIN_MEMORY_PARAM_UNLIMITED);
#endif

#if HAVE_VIRDOMAINSETMEMORYFLAGS
    rb_define_const(c_domain, "DOMAIN_MEM_LIVE", INT2NUM(VIR_DOMAIN_MEM_LIVE));
    rb_define_const(c_domain, "DOMAIN_MEM_CONFIG",
                    INT2NUM(VIR_DOMAIN_MEM_CONFIG));
#endif
#if HAVE_CONST_VIR_DOMAIN_MEM_CURRENT
    rb_define_const(c_domain, "DOMAIN_MEM_CURRENT",
                    INT2NUM(VIR_DOMAIN_MEM_CURRENT));
    rb_define_const(c_domain, "DOMAIN_MEM_MAXIMUM",
                    INT2NUM(VIR_DOMAIN_MEM_MAXIMUM));
#endif

    rb_define_method(c_domain, "scheduler_parameters",
                     libvirt_dom_get_scheduler_parameters, -1);
    rb_define_method(c_domain, "scheduler_parameters=",
                     libvirt_dom_set_scheduler_parameters, 1);

#if HAVE_VIRDOMAINSETMEMORYPARAMETERS
    rb_define_method(c_domain, "memory_parameters",
                     libvirt_dom_get_memory_parameters, -1);
    rb_define_method(c_domain, "memory_parameters=",
                     libvirt_dom_set_memory_parameters, 1);
#endif

#if HAVE_VIRDOMAINSETBLKIOPARAMETERS
    rb_define_method(c_domain, "blkio_parameters",
                     libvirt_dom_get_blkio_parameters, -1);
    rb_define_method(c_domain, "blkio_parameters=",
                     libvirt_dom_set_blkio_parameters, 1);
#endif

#if HAVE_VIRDOMAINGETSTATE
    rb_define_const(c_domain, "DOMAIN_RUNNING_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_RUNNING_UNKNOWN));
    rb_define_const(c_domain, "DOMAIN_RUNNING_BOOTED",
                    INT2NUM(VIR_DOMAIN_RUNNING_BOOTED));
    rb_define_const(c_domain, "DOMAIN_RUNNING_MIGRATED",
                    INT2NUM(VIR_DOMAIN_RUNNING_MIGRATED));
    rb_define_const(c_domain, "DOMAIN_RUNNING_RESTORED",
                    INT2NUM(VIR_DOMAIN_RUNNING_RESTORED));
    rb_define_const(c_domain, "DOMAIN_RUNNING_FROM_SNAPSHOT",
                    INT2NUM(VIR_DOMAIN_RUNNING_FROM_SNAPSHOT));
    rb_define_const(c_domain, "DOMAIN_RUNNING_UNPAUSED",
                    INT2NUM(VIR_DOMAIN_RUNNING_UNPAUSED));
    rb_define_const(c_domain, "DOMAIN_RUNNING_MIGRATION_CANCELED",
                    INT2NUM(VIR_DOMAIN_RUNNING_MIGRATION_CANCELED));
    rb_define_const(c_domain, "DOMAIN_RUNNING_SAVE_CANCELED",
                    INT2NUM(VIR_DOMAIN_RUNNING_SAVE_CANCELED));
#if HAVE_CONST_VIR_DOMAIN_RUNNING_WAKEUP
    rb_define_const(c_domain, "DOMAIN_RUNNING_WAKEUP",
                    INT2NUM(VIR_DOMAIN_RUNNING_WAKEUP));
#endif
    rb_define_const(c_domain, "DOMAIN_BLOCKED_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_BLOCKED_UNKNOWN));
    rb_define_const(c_domain, "DOMAIN_PAUSED_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_PAUSED_UNKNOWN));
    rb_define_const(c_domain, "DOMAIN_PAUSED_USER",
                    INT2NUM(VIR_DOMAIN_PAUSED_USER));
    rb_define_const(c_domain, "DOMAIN_PAUSED_MIGRATION",
                    INT2NUM(VIR_DOMAIN_PAUSED_MIGRATION));
    rb_define_const(c_domain, "DOMAIN_PAUSED_SAVE",
                    INT2NUM(VIR_DOMAIN_PAUSED_SAVE));
    rb_define_const(c_domain, "DOMAIN_PAUSED_DUMP",
                    INT2NUM(VIR_DOMAIN_PAUSED_DUMP));
    rb_define_const(c_domain, "DOMAIN_PAUSED_IOERROR",
                    INT2NUM(VIR_DOMAIN_PAUSED_IOERROR));
    rb_define_const(c_domain, "DOMAIN_PAUSED_WATCHDOG",
                    INT2NUM(VIR_DOMAIN_PAUSED_WATCHDOG));
    rb_define_const(c_domain, "DOMAIN_PAUSED_FROM_SNAPSHOT",
                    INT2NUM(VIR_DOMAIN_PAUSED_FROM_SNAPSHOT));
#if HAVE_CONST_VIR_DOMAIN_PAUSED_SHUTTING_DOWN
    rb_define_const(c_domain, "DOMAIN_PAUSED_SHUTTING_DOWN",
                    INT2NUM(VIR_DOMAIN_PAUSED_SHUTTING_DOWN));
#endif
#if HAVE_CONST_VIR_DOMAIN_PAUSED_SNAPSHOT
    rb_define_const(c_domain, "DOMAIN_PAUSED_SNAPSHOT",
                    INT2NUM(VIR_DOMAIN_PAUSED_SNAPSHOT));
#endif
    rb_define_const(c_domain, "DOMAIN_SHUTDOWN_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_UNKNOWN));
    rb_define_const(c_domain, "DOMAIN_SHUTDOWN_USER",
                    INT2NUM(VIR_DOMAIN_SHUTDOWN_USER));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_UNKNOWN));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_SHUTDOWN",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_SHUTDOWN));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_DESTROYED",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_DESTROYED));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_CRASHED",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_CRASHED));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_MIGRATED",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_MIGRATED));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_SAVED",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_SAVED));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_FAILED",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_FAILED));
    rb_define_const(c_domain, "DOMAIN_SHUTOFF_FROM_SNAPSHOT",
                    INT2NUM(VIR_DOMAIN_SHUTOFF_FROM_SNAPSHOT));
    rb_define_const(c_domain, "DOMAIN_CRASHED_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_CRASHED_UNKNOWN));
#if HAVE_CONST_VIR_DOMAIN_PMSUSPENDED_UNKNOWN
    rb_define_const(c_domain, "DOMAIN_PMSUSPENDED_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_PMSUSPENDED_UNKNOWN));
#endif
#if HAVE_CONST_VIR_DOMAIN_PMSUSPENDED_DISK_UNKNOWN
    rb_define_const(c_domain, "DOMAIN_PMSUSPENDED_DISK_UNKNOWN",
                    INT2NUM(VIR_DOMAIN_PMSUSPENDED_DISK_UNKNOWN));
#endif

    rb_define_method(c_domain, "state", libvirt_dom_get_state, -1);
#endif

#if HAVE_CONST_VIR_DOMAIN_AFFECT_CURRENT
    rb_define_const(c_domain, "DOMAIN_AFFECT_CURRENT",
                    INT2NUM(VIR_DOMAIN_AFFECT_CURRENT));
    rb_define_const(c_domain, "DOMAIN_AFFECT_LIVE",
                    INT2NUM(VIR_DOMAIN_AFFECT_LIVE));
    rb_define_const(c_domain, "DOMAIN_AFFECT_CONFIG",
                    INT2NUM(VIR_DOMAIN_AFFECT_CONFIG));
#endif

#if HAVE_VIRDOMAINOPENCONSOLE
    rb_define_method(c_domain, "open_console", libvirt_dom_open_console, -1);
#endif

#if HAVE_VIRDOMAINSCREENSHOT
    rb_define_method(c_domain, "screenshot", libvirt_dom_screenshot, -1);
#endif

#if HAVE_VIRDOMAININJECTNMI
    rb_define_method(c_domain, "inject_nmi", libvirt_dom_inject_nmi, -1);
#endif

#if HAVE_VIRDOMAINGETCONTROLINFO
    /*
     * Class Libvirt::Domain::ControlInfo
     */
    c_domain_control_info = rb_define_class_under(c_domain, "ControlInfo",
                                                  rb_cObject);
    rb_define_attr(c_domain_control_info, "state", 1, 0);
    rb_define_attr(c_domain_control_info, "details", 1, 0);
    rb_define_attr(c_domain_control_info, "stateTime", 1, 0);

    rb_define_const(c_domain_control_info, "CONTROL_OK",
                    INT2NUM(VIR_DOMAIN_CONTROL_OK));
    rb_define_const(c_domain_control_info, "CONTROL_JOB",
                    INT2NUM(VIR_DOMAIN_CONTROL_JOB));
    rb_define_const(c_domain_control_info, "CONTROL_OCCUPIED",
                    INT2NUM(VIR_DOMAIN_CONTROL_OCCUPIED));
    rb_define_const(c_domain_control_info, "CONTROL_ERROR",
                    INT2NUM(VIR_DOMAIN_CONTROL_ERROR));

    rb_define_method(c_domain, "control_info", libvirt_dom_control_info, -1);
#endif

#if HAVE_VIRDOMAINMIGRATEGETMAXSPEED
    rb_define_method(c_domain, "migrate_max_speed",
                     libvirt_dom_migrate_max_speed, -1);
#endif
#if HAVE_VIRDOMAINSENDKEY
    rb_define_method(c_domain, "send_key", libvirt_dom_send_key, 3);
#endif
#if HAVE_VIRDOMAINRESET
    rb_define_method(c_domain, "reset", libvirt_dom_reset, -1);
#endif
#if HAVE_VIRDOMAINGETHOSTNAME
    rb_define_method(c_domain, "hostname", libvirt_dom_hostname, -1);
#endif
#if HAVE_VIRDOMAINGETMETADATA
    rb_define_const(c_domain, "METADATA_DESCRIPTION",
                    INT2NUM(VIR_DOMAIN_METADATA_DESCRIPTION));
    rb_define_const(c_domain, "METADATA_TITLE",
                    INT2NUM(VIR_DOMAIN_METADATA_TITLE));
    rb_define_const(c_domain, "METADATA_ELEMENT",
                    INT2NUM(VIR_DOMAIN_METADATA_ELEMENT));
    rb_define_method(c_domain, "metadata", libvirt_dom_metadata, -1);
#endif
#if HAVE_VIRDOMAINSETMETADATA
    rb_define_method(c_domain, "metadata=", libvirt_dom_set_metadata, 1);
#endif
#if HAVE_VIRDOMAINPROCESSSIGNAL
    rb_define_const(c_domain, "PROCESS_SIGNAL_NOP",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_NOP));
    rb_define_const(c_domain, "PROCESS_SIGNAL_HUP",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_HUP));
    rb_define_const(c_domain, "PROCESS_SIGNAL_INT",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_INT));
    rb_define_const(c_domain, "PROCESS_SIGNAL_QUIT",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_QUIT));
    rb_define_const(c_domain, "PROCESS_SIGNAL_ILL",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_ILL));
    rb_define_const(c_domain, "PROCESS_SIGNAL_TRAP",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_TRAP));
    rb_define_const(c_domain, "PROCESS_SIGNAL_ABRT",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_ABRT));
    rb_define_const(c_domain, "PROCESS_SIGNAL_BUS",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_BUS));
    rb_define_const(c_domain, "PROCESS_SIGNAL_FPE",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_FPE));
    rb_define_const(c_domain, "PROCESS_SIGNAL_KILL",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_KILL));
    rb_define_const(c_domain, "PROCESS_SIGNAL_USR1",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_USR1));
    rb_define_const(c_domain, "PROCESS_SIGNAL_SEGV",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_SEGV));
    rb_define_const(c_domain, "PROCESS_SIGNAL_USR2",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_USR2));
    rb_define_const(c_domain, "PROCESS_SIGNAL_PIPE",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_PIPE));
    rb_define_const(c_domain, "PROCESS_SIGNAL_ALRM",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_ALRM));
    rb_define_const(c_domain, "PROCESS_SIGNAL_TERM",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_TERM));
    rb_define_const(c_domain, "PROCESS_SIGNAL_STKFLT",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_STKFLT));
    rb_define_const(c_domain, "PROCESS_SIGNAL_CHLD",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_CHLD));
    rb_define_const(c_domain, "PROCESS_SIGNAL_CONT",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_CONT));
    rb_define_const(c_domain, "PROCESS_SIGNAL_STOP",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_STOP));
    rb_define_const(c_domain, "PROCESS_SIGNAL_TSTP",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_TSTP));
    rb_define_const(c_domain, "PROCESS_SIGNAL_TTIN",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_TTIN));
    rb_define_const(c_domain, "PROCESS_SIGNAL_TTOU",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_TTOU));
    rb_define_const(c_domain, "PROCESS_SIGNAL_URG",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_URG));
    rb_define_const(c_domain, "PROCESS_SIGNAL_XCPU",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_XCPU));
    rb_define_const(c_domain, "PROCESS_SIGNAL_XFSZ",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_XFSZ));
    rb_define_const(c_domain, "PROCESS_SIGNAL_VTALRM",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_VTALRM));
    rb_define_const(c_domain, "PROCESS_SIGNAL_PROF",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_PROF));
    rb_define_const(c_domain, "PROCESS_SIGNAL_WINCH",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_WINCH));
    rb_define_const(c_domain, "PROCESS_SIGNAL_POLL",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_POLL));
    rb_define_const(c_domain, "PROCESS_SIGNAL_PWR",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_PWR));
    rb_define_const(c_domain, "PROCESS_SIGNAL_SYS",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_SYS));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT0",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT0));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT1",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT1));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT2",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT2));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT3",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT3));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT4",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT4));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT5",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT5));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT6",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT6));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT7",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT7));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT8",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT8));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT9",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT9));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT10",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT10));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT11",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT11));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT12",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT12));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT13",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT13));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT14",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT14));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT15",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT15));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT16",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT16));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT17",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT17));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT18",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT18));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT19",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT19));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT20",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT20));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT21",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT21));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT22",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT22));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT23",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT23));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT24",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT24));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT25",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT25));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT26",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT26));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT27",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT27));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT28",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT28));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT29",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT29));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT30",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT30));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT31",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT31));
    rb_define_const(c_domain, "PROCESS_SIGNAL_RT32",
                    INT2NUM(VIR_DOMAIN_PROCESS_SIGNAL_RT32));
    rb_define_method(c_domain, "send_process_signal",
                     libvirt_dom_send_process_signal, -1);
#endif
#if HAVE_VIRDOMAINLISTALLSNAPSHOTS
    rb_define_const(c_domain_snapshot, "LIST_ROOTS",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_ROOTS));
    rb_define_const(c_domain_snapshot, "LIST_DESCENDANTS",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_DESCENDANTS));
    rb_define_const(c_domain_snapshot, "LIST_LEAVES",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_LEAVES));
    rb_define_const(c_domain_snapshot, "LIST_NO_LEAVES",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_NO_LEAVES));
    rb_define_const(c_domain_snapshot, "LIST_METADATA",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_METADATA));
    rb_define_const(c_domain_snapshot, "LIST_NO_METADATA",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_NO_METADATA));
#if HAVE_CONST_VIR_DOMAIN_SNAPSHOT_LIST_INACTIVE
    rb_define_const(c_domain_snapshot, "LIST_INACTIVE",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_INACTIVE));
    rb_define_const(c_domain_snapshot, "LIST_ACTIVE",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_ACTIVE));
    rb_define_const(c_domain_snapshot, "LIST_DISK_ONLY",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_DISK_ONLY));
    rb_define_const(c_domain_snapshot, "LIST_INTERNAL",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_INTERNAL));
    rb_define_const(c_domain_snapshot, "LIST_EXTERNAL",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_LIST_EXTERNAL));
#endif
    rb_define_method(c_domain, "list_all_snapshots",
                     libvirt_dom_list_all_snapshots, -1);
#endif
#if HAVE_CONST_VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE
    rb_define_const(c_domain_snapshot, "CREATE_REDEFINE",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_REDEFINE));
    rb_define_const(c_domain_snapshot, "CREATE_CURRENT",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_CURRENT));
    rb_define_const(c_domain_snapshot, "CREATE_NO_METADATA",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_NO_METADATA));
    rb_define_const(c_domain_snapshot, "CREATE_HALT",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_HALT));
    rb_define_const(c_domain_snapshot, "CREATE_DISK_ONLY",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY));
    rb_define_const(c_domain_snapshot, "CREATE_REUSE_EXT",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_REUSE_EXT));
    rb_define_const(c_domain_snapshot, "CREATE_QUIESCE",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_QUIESCE));
    rb_define_const(c_domain_snapshot, "CREATE_ATOMIC",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_ATOMIC));
#endif
#if HAVE_CONST_VIR_DOMAIN_SNAPSHOT_CREATE_LIVE
    rb_define_const(c_domain_snapshot, "CREATE_LIVE",
                    INT2NUM(VIR_DOMAIN_SNAPSHOT_CREATE_LIVE));
#endif
#if HAVE_VIRDOMAINSNAPSHOTNUMCHILDREN
    rb_define_method(c_domain_snapshot, "num_children",
                     libvirt_dom_snapshot_num_children, -1);
#endif
#if HAVE_VIRDOMAINSNAPSHOTLISTCHILDRENNAMES
    rb_define_method(c_domain_snapshot, "list_children_names",
                     libvirt_dom_snapshot_list_children_names, -1);
#endif
#if HAVE_VIRDOMAINSNAPSHOTLISTALLCHILDREN
    rb_define_method(c_domain_snapshot, "list_all_children",
                     libvirt_dom_snapshot_list_all_children, -1);
#endif
#if HAVE_VIRDOMAINSNAPSHOTGETPARENT
    rb_define_method(c_domain_snapshot, "parent",
                     libvirt_dom_snapshot_parent, -1);
#endif
#if HAVE_VIRDOMAINSNAPSHOTISCURRENT
    rb_define_method(c_domain_snapshot, "current?",
                     libvirt_domain_snapshot_current_p, -1);
#endif
#if HAVE_VIRDOMAINSNAPSHOTHASMETADATA
    rb_define_method(c_domain_snapshot, "has_metadata?",
                     libvirt_domain_snapshot_has_metadata_p, -1);
#endif
}
