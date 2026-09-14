#include <string.h>

#include "quickjs.h"

#include "telephony_common.h"



static JSClassID js_telephony_class_id;



typedef struct {

    int active;

} JSTelephonyContext;



static void js_telephony_finalizer(SRuntime *rt, JSValue val) {

    JSTelephonyContext *ctx = JS_GetOpaque(val, js_telephony_class_id);

    if (ctx) {

        js_free_rt(rt, ctx);

    }

}



// js_telephony_get_cells(ctx, this_val, argc, argv)

static JSValue js_telephony_get_cells(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {

    JSTelephonyContext *sh = JS_GetOpaque2(ctx, this_val, js_telephony_class_id);

    if (!sh) return JS_EXCEPTION;



    CellTowerMetric cells[8];

    memset(cells, 0, sizeof(cells));

    int count = 0;



    // Simulate population via dynamic parsing or routes

    telephony_parse_registry_dumpsys("CellIdentityLte:{mMcc=310 mMnc=260 mCi=2390812 mPci=312 mTac=14232 mEarfcn=66661}", cells, 8, &count);



    JSValue arr = JS_NewArray(ctx);

    for (int i = 0; i < count; i++) {

        JSValue obj = JS_NewObject(ctx);

        JS_SetPropertyStr(ctx, obj, "type", JS_NewInt32(ctx, cells[i].type));

        JS_SetPropertyStr(ctx, obj, "status", JS_NewInt32(ctx, cells[i].status));

        JS_SetPropertyStr(ctx, obj, "dbm", JS_NewInt32(ctx, cells[i].dbm));

        JS_SetPropertyStr(ctx, obj, "rsrp", JS_NewInt32(ctx, cells[i].rsrp));

        JS_SetPropertyStr(ctx, obj, "rsrq", JS_NewInt32(ctx, cells[i].rsrq));

        JS_SetPropertyStr(ctx, obj, "rssnr", JS_NewInt32(ctx, cells[i].rssnr));

        JS_SetPropertyStr(ctx, obj, "mcc", JS_NewInt32(ctx, cells[i].mcc));

        JS_SetPropertyStr(ctx, obj, "mnc", JS_NewInt32(ctx, cells[i].mnc));

        JS_SetPropertyStr(ctx, obj, "lac_or_tac", JS_NewInt32(ctx, cells[i].lac_or_tac));

        JS_SetPropertyStr(ctx, obj, "cid_or_ci", JS_NewInt32(ctx, cells[i].cid_or_ci));

        JS_SetPropertyStr(ctx, obj, "pci_or_psc", JS_NewInt32(ctx, cells[i].pci_or_psc));

        JS_SetPropertyStr(ctx, obj, "earfcn", JS_NewInt32(ctx, cells[i].earfcn_or_nrarfcn));



        JS_SetPropertyUint32(ctx, arr, i, obj);

    }



    return arr;

}



static const JSCFunctionListEntry js_telephony_proto_funcs[] = {

    JS_CFUNC_DEF("getCells", 0, js_telephony_get_cells),

};



static int js_telephony_init(JSContext *ctx, JSModuleDef *m) {

    JS_NewClassID(&js_telephony_class_id);

    JSClassDef class_def = {

        "Telephony",

        .finalizer = js_telephony_finalizer,

    };

    JS_NewClass(JS_GetRuntime(ctx), js_telephony_class_id, &class_def);



    JSValue proto = JS_NewObject(ctx);

    JS_SetPropertyFunctionList(ctx, proto, js_telephony_proto_funcs, sizeof(js_telephony_proto_funcs)/sizeof(JSCFunctionListEntry));

    JS_SetClassProto(ctx, js_telephony_class_id, proto);



    return 0;

}