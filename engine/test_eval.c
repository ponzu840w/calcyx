/* 移植元: Calctus/Samples/Test_*.txt を eval_str() で自動実行するテストハーネス */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "eval/eval.h"
#include "eval/eval_ctx.h"

#define MAX_LINE 4096

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: test_eval <testfile.txt>\n");
        return 1;
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "Cannot open: %s\n", argv[1]);
        return 1;
    }

    eval_ctx_t ctx;
    eval_ctx_init(&ctx);

    int pass = 0, fail = 0;
    int lineno = 0;
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        /* 末尾の改行・空白を除去 */
        int len = (int)strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'
                           || line[len-1] == ' ' || line[len-1] == '\t'))
            line[--len] = '\0';

        /* 空行・コメント行はスキップ (`;` は移植元で eval 側、`/` は当ハーネスのみ採用) */
        if (len == 0 || line[0] == ';' || line[0] == '/')
            continue;

        /* エラー状態をリセット */
        ctx.has_error = false;
        ctx.error_msg[0] = '\0';

        char errmsg[512] = "";
        val_t *result = eval_str(line, &ctx, errmsg, sizeof(errmsg));

        /* エラー判定 */
        bool ok = true;
        const char *fail_reason = NULL;

        if (ctx.has_error) {
            ok = false;
            fail_reason = ctx.error_msg[0] ? ctx.error_msg : "error";
            ctx.has_error = false;
            ctx.error_msg[0] = '\0';
        } else if (!result) {
            ok = false;
            fail_reason = errmsg[0] ? errmsg : "null result";
        } else if (result->type == VAL_BOOL && !result->bool_v) {
            ok = false;
            fail_reason = "false";
        }

        if (result) val_free(result);

        if (ok) {
            pass++;
        } else {
            fail++;
            fprintf(stderr, "FAIL [%s:%d] %s\n  => %s\n",
                    argv[1], lineno, line,
                    fail_reason ? fail_reason : "(unknown)");
        }
    }

    fclose(fp);
    eval_ctx_free(&ctx);

    printf("%s: %d passed, %d failed\n", argv[1], pass, fail);
    return fail > 0 ? 1 : 0;
}
