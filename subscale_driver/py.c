#include "py.h"
#include "lib/pf_string.h"

#include <osdefs.h>
#include <frameobject.h>

struct py_err_ctx{
    bool           occurred;
    PyObject      *type;
    PyObject      *value;
    PyObject      *traceback;
};

static void s_err_clear(struct py_err_ctx* s_err_ctx)
{
    Py_CLEAR(s_err_ctx->type);
    Py_CLEAR(s_err_ctx->value);
    Py_CLEAR(s_err_ctx->traceback);
    s_err_ctx->occurred = false;
}

int s_parse_syntax_error(PyObject *err, PyObject **message, const char **filename, 
                                 int *lineno, int *offset, const char **text)
{
    long hold;
    PyObject *v;

    /* old style errors */
    if (PyTuple_Check(err))
        return PyArg_ParseTuple(err, "O(ziiz)", message, filename,
                                lineno, offset, text);

    *message = NULL;

    /* new style errors.  `err' is an instance */
    *message = PyObject_GetAttrString(err, "msg");
    if (!*message)
        goto finally;

    v = PyObject_GetAttrString(err, "filename");
    if (!v)
        goto finally;
    if (v == Py_None) {
        Py_DECREF(v);
        *filename = NULL;
    }
    else {
      #define PyString_AsString PyUnicode_AsUTF8
        *filename = PyString_AsString(v);
        Py_DECREF(v);
        if (!*filename)
            goto finally;
    }

    v = PyObject_GetAttrString(err, "lineno");
    if (!v)
        goto finally;
    #define PyInt_AsLong PyLong_AsLong
    hold = PyInt_AsLong(v);
    Py_DECREF(v);
    if (hold < 0 && PyErr_Occurred())
        goto finally;
    *lineno = (int)hold;

    v = PyObject_GetAttrString(err, "offset");
    if (!v)
        goto finally;
    if (v == Py_None) {
        *offset = -1;
        Py_DECREF(v);
    } else {
        hold = PyInt_AsLong(v);
        Py_DECREF(v);
        if (hold < 0 && PyErr_Occurred())
            goto finally;
        *offset = (int)hold;
    }

    v = PyObject_GetAttrString(err, "text");
    if (!v)
        goto finally;
    if (v == Py_None) {
        Py_DECREF(v);
        *text = NULL;
    }
    else {
        *text = PyString_AsString(v);
        Py_DECREF(v);
        if (!*text)
            goto finally;
    }
    return 1;

finally:
    Py_XDECREF(*message);
    return 0;
}

void S_Error_Update(struct py_err_ctx *err_ctx) {
    // Handle with command-line too: printf's below //

    const char* msg = "Unhandled Python Exception";
    printf("%s\n", msg);
    if (true) {
        char buff[256] = "";
        PyObject *repr;

        assert(err_ctx->type);
        if(PyExceptionClass_Check(err_ctx->type) && PyExceptionClass_Name(err_ctx->type)) {
            const char *clsname = PyExceptionClass_Name(err_ctx->type);
            char *dot = strrchr(clsname, '.');
            if(dot)
                clsname = dot+1;
            pf_strlcpy(buff, clsname, sizeof(buff));
        }else{
            repr = PyObject_Str(err_ctx->type);
            if(repr) {
                pf_strlcpy(buff, PyString_AS_STRING(repr), sizeof(buff));
            }
            Py_XDECREF(repr);
        }

        PyObject *message = NULL;
        const char *filename, *text;
        int lineno, offset;

        if(err_ctx->value) {

            if(s_parse_syntax_error(err_ctx->value, &message, &filename, &lineno, &offset, &text)) {
                repr = PyObject_Str(message);
                Py_DECREF(message);
            }else{
                PyErr_Clear();
                repr = PyObject_Str(err_ctx->value);
            }

            if(repr && strlen(PyString_AS_STRING(repr))) {
                pf_strlcat(buff, ": ", sizeof(buff));
                pf_strlcat(buff, PyString_AS_STRING(repr), sizeof(buff));
            }
            Py_XDECREF(repr);
        }

	printf("%s\n", buff);

        if(message) {

            pf_snprintf(buff, sizeof(buff), "    File: \"%s\"", filename ? filename : "<string>", lineno);
	    printf("%s\n", buff);

            pf_snprintf(buff, sizeof(buff), "    Line: %d", lineno);
	    printf("%s\n", buff);

            if(text) {
            
                s_print_err_text(offset, text, sizeof(buff), buff);

                int idx = 0;
                char *saveptr;
                char *curr = pf_strtok_r(buff, "\n", &saveptr);
                while(curr) {
		    printf("%s\n", curr);
                    curr = pf_strtok_r(NULL, "\n", &saveptr);
                    idx++;
                }
            }
        }
        Py_XDECREF(message);

        if(err_ctx->traceback) {
        
	    const char* msg = "Traceback:";
	    printf("%s\n", msg);

            long depth = 0;
            PyTracebackObject *tb = (PyTracebackObject*)err_ctx->traceback, *tbLast;
	    PyTracebackObject *lastTB = tb;

            while (tb != NULL) {
                depth++;
                tb = tb->tb_next;
		lastTB = tb;
            }
            tb = (PyTracebackObject*)err_ctx->traceback;
	    lastTB = tb;

            while (tb != NULL) {
                if (depth <= 128) {

                    char filebuff[512] = "";
                    pf_snprintf(filebuff, sizeof(filebuff), "  [%02ld] %s: %d", depth,
                        PyString_AsString(tb->tb_frame->f_code->co_filename), tb->tb_lineno);
		    printf("%s\n", filebuff);

                    char linebuff[512] = "";
                    s_print_source_line(PyString_AsString(tb->tb_frame->f_code->co_filename), tb->tb_lineno, 4, 
                        sizeof(linebuff), linebuff);
                    size_t len = strlen(linebuff);
                    linebuff[len > 0 ? len-1 : 0] = '\0'; /* trim newline */

		    printf("%s\n", linebuff);
                }
                depth--;
		//lastTB = tb;
                tb = tb->tb_next;
            }

	    // // Grab locals on last frame
	    // if (lastTB != NULL) {
	    //   PyObject* locals = lastTB->tb_frame->f_locals;//f_frame->f_locals;
	    //   S_RunString("import code; code.interact(local=locals())", locals);
	    //   // If we leave the above, consider it "continue"'ed:
	    //   err_ctx->occurred = false;
	    //   Py_CLEAR(err_ctx->type);
	    //   Py_CLEAR(err_ctx->value);
	    //   Py_CLEAR(err_ctx->traceback);
	    //   G_SetSimState(err_ctx->prev_state);
	    // }
        }

        // if(nk_button_label(ctx, "Continue")) {

        //     err_ctx->occurred = false;
        //     Py_CLEAR(err_ctx->type);
        //     Py_CLEAR(err_ctx->value);
        //     Py_CLEAR(err_ctx->traceback);
        //     G_SetSimState(err_ctx->prev_state);
        // }

    }
}

void S_ShowLastError(void)
{
    static struct py_err_ctx  s_err_ctx = {false,};
    s_err_ctx.occurred = PyErr_Occurred();
    PyErr_Fetch(&s_err_ctx.type, &s_err_ctx.value, &s_err_ctx.traceback);
    PyErr_NormalizeException(&s_err_ctx.type, &s_err_ctx.value, &s_err_ctx.traceback);

    if(s_err_ctx.occurred) {
      S_Error_Update(&s_err_ctx);
      s_err_clear(&s_err_ctx);
    }
}