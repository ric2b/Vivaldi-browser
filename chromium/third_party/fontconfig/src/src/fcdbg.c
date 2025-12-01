/*
 * fontconfig/src/fcdbg.c
 *
 * Copyright © 2000 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the author(s) not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The authors make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE AUTHOR(S) DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "fcint.h"

#include <stdio.h>
#include <stdlib.h>

static void
_FcValuePrintFile (FILE *stream, const FcValue v)
{
    switch (v.type) {
    case FcTypeUnknown:
	fprintf (stream, "<unknown>");
	break;
    case FcTypeVoid:
	fprintf (stream, "<void>");
	break;
    case FcTypeInteger:
	fprintf (stream, "%d(i)", v.u.i);
	break;
    case FcTypeDouble:
	fprintf (stream, "%g(f)", v.u.d);
	break;
    case FcTypeString:
	fprintf (stream, "\"%s\"", v.u.s);
	break;
    case FcTypeBool:
	fprintf (stream,
	         v.u.b == FcTrue ? "True" : v.u.b == FcFalse ? "False"
	                                                     : "DontCare");
	break;
    case FcTypeMatrix:
	fprintf (stream, "[%g %g; %g %g]", v.u.m->xx, v.u.m->xy, v.u.m->yx, v.u.m->yy);
	break;
    case FcTypeCharSet:
	FcCharSetPrintFile (stream, v.u.c);
	break;
    case FcTypeLangSet:
	FcLangSetPrint (v.u.l);
	break;
    case FcTypeFTFace:
	fprintf (stream, "face");
	break;
    case FcTypeRange:
	fprintf (stream, "[%g %g]", v.u.r->begin, v.u.r->end);
	break;
    }
}

void
FcValuePrintFile (FILE *stream, const FcValue v)
{
    fprintf (stream, " ");
    _FcValuePrintFile (stream, v);
}

void
FcValuePrint (const FcValue v)
{
    printf (" ");
    _FcValuePrintFile (stdout, v);
}

void
FcValuePrintFileWithPosition (FILE *stream, const FcValue v, FcBool show_pos_mark)
{
    if (show_pos_mark)
	fprintf (stream, " [marker] ");
    else
	fprintf (stream, " ");
    _FcValuePrintFile (stream, v);
}

void
FcValuePrintWithPosition (const FcValue v, FcBool show_pos_mark)
{
    FcValuePrintFileWithPosition (stdout, v, show_pos_mark);
}

static void
FcValueBindingPrintFile (FILE *stream, const FcValueListPtr l)
{
    switch (l->binding) {
    case FcValueBindingWeak:
	fprintf (stream, "(w)");
	break;
    case FcValueBindingStrong:
	fprintf (stream, "(s)");
	break;
    case FcValueBindingSame:
	fprintf (stream, "(=)");
	break;
    default:
	/* shouldn't be reached */
	fprintf (stream, "(?)");
	break;
    }
}

void
FcValueListPrintFileWithPosition (FILE *stream, FcValueListPtr l, const FcValueListPtr pos)
{
    for (; l != NULL; l = FcValueListNext (l)) {
	FcValuePrintFileWithPosition (stream, FcValueCanonicalize (&l->value), pos != NULL && l == pos);
	FcValueBindingPrintFile (stream, l);
    }
    if (!pos)
	fprintf (stream, " [marker]");
}

void
FcValueListPrintWithPosition (FcValueListPtr l, const FcValueListPtr pos)
{
    FcValueListPrintFileWithPosition(stdout, l, pos);
}

void
FcValueListPrintFile (FILE *stream, FcValueListPtr l)
{
    for (; l != NULL; l = FcValueListNext (l)) {
	FcValuePrintFile (stream, FcValueCanonicalize (&l->value));
	FcValueBindingPrintFile (stream, l);
    }
}

void
FcValueListPrint (FcValueListPtr l)
{
    FcValueListPrintFile (stdout, l);
}

void
FcLangSetPrintFile (FILE *stream, const FcLangSet *ls)
{
    FcStrBuf buf;
    FcChar8  init_buf[1024];

    FcStrBufInit (&buf, init_buf, sizeof (init_buf));
    if (FcNameUnparseLangSet (&buf, ls) && FcStrBufChar (&buf, '\0'))
	fprintf (stream, "%s", buf.buf);
    else
	fprintf (stream, "langset (alloc error)");
    FcStrBufDestroy (&buf);
}

void
FcLangSetPrint (const FcLangSet *ls)
{
    FcLangSetPrintFile (stdout, ls);
}

void
FcCharSetPrintFile (FILE *stream, const FcCharSet *c)
{
    int       i, j;
    intptr_t *leaves = FcCharSetLeaves (c);
    FcChar16 *numbers = FcCharSetNumbers (c);

#if 0
    fprintf (stream, "CharSet  0x%x\n", (intptr_t) c);
    fprintf (stream, "Leaves:  +%d = 0x%x\n", c->leaves_offset, (intptr_t) leaves);
    fprintf (stream, "Numbers: +%d = 0x%x\n", c->numbers_offset, (intptr_t) numbers);

    for (i = 0; i < c->num; i++)
    {
	fprintf (stream, "Page %d: %04x +%d = 0x%x\n",
		 i, numbers[i], leaves[i],
		 (intptr_t) FcOffsetToPtr (leaves, leaves[i], FcCharLeaf));
    }
#endif

    fprintf (stream, "\n");
    for (i = 0; i < c->num; i++) {
	intptr_t    leaf_offset = leaves[i];
	FcCharLeaf *leaf = FcOffsetToPtr (leaves, leaf_offset, FcCharLeaf);

	fprintf (stream, "\t");
	fprintf (stream, "%04x:", numbers[i]);
	for (j = 0; j < 256 / 32; j++)
	    fprintf (stream, " %08x", leaf->map[j]);
	fprintf (stream, "\n");
    }
}

void
FcCharSetPrint (const FcCharSet *c)
{
    FcCharSetPrintFile (stdout, c);
}

void
FcPatternPrintFile (FILE *stream, const FcPattern *p)
{
    FcPatternIter iter;

    if (!p) {
	fprintf (stream, "Null pattern\n");
	return;
    }
    fprintf (stream, "Pattern has %d elts (size %d)\n", FcPatternObjectCount (p), p->size);
    FcPatternIterStart (p, &iter);
    do {
	fprintf (stream, "\t%s:", FcPatternIterGetObject (p, &iter));
	FcValueListPrintFile (stream, FcPatternIterGetValues (p, &iter));
	fprintf (stream, "\n");
    } while (FcPatternIterNext (p, &iter));
    fprintf (stream, "\n");
}

void
FcPatternPrint (const FcPattern *p)
{
    FcPatternPrintFile (stdout, p);
}

#define FcOpFlagsPrintFile(_f_, _o_)          \
    {                                         \
	int f = FC_OP_GET_FLAGS (_o_);        \
	if (f & FcOpFlagIgnoreBlanks)         \
	    fprintf (_f_, "(ignore blanks)"); \
    }

#define FcOpFlagsPrint(_o_)    FcOpFlagsPrintFile(stdout, _o_)

void
FcPatternPrint2File (FILE              *stream,
                     FcPattern         *pp1,
                     FcPattern         *pp2,
                     const FcObjectSet *os)
{
    int           i, j, k, pos;
    FcPatternElt *e1, *e2;
    FcPattern    *p1, *p2;

    if (os) {
	p1 = FcPatternFilter (pp1, os);
	p2 = FcPatternFilter (pp2, os);
    } else {
	p1 = pp1;
	p2 = pp2;
    }
    fprintf (stream, "Pattern has %d elts (size %d), %d elts (size %d)\n",
             p1->num, p1->size, p2->num, p2->size);
    for (i = 0, j = 0; i < p1->num; i++) {
	e1 = &FcPatternElts (p1)[i];
	e2 = &FcPatternElts (p2)[j];
	if (!e2 || e1->object != e2->object) {
	    pos = FcPatternPosition (p2, FcObjectName (e1->object));
	    if (pos >= 0) {
		for (k = j; k < pos; k++) {
		    e2 = &FcPatternElts (p2)[k];
		    fprintf (stream, "\t%s: (None) -> ", FcObjectName (e2->object));
		    FcValueListPrintFile (stream, FcPatternEltValues (e2));
		    fprintf (stream, "\n");
		}
		j = pos;
		goto cont;
	    } else {
		fprintf (stream, "\t%s:", FcObjectName (e1->object));
		FcValueListPrintFile (stream, FcPatternEltValues (e1));
		fprintf (stream, " -> (None)\n");
	    }
	} else {
	cont:
	    fprintf (stream, "\t%s:", FcObjectName (e1->object));
	    FcValueListPrintFile (stream, FcPatternEltValues (e1));
	    fprintf (stream, " -> ");
	    e2 = &FcPatternElts (p2)[j];
	    FcValueListPrintFile (stream, FcPatternEltValues (e2));
	    fprintf (stream, "\n");
	    j++;
	}
    }
    if (j < p2->num) {
	for (k = j; k < p2->num; k++) {
	    e2 = &FcPatternElts (p2)[k];
	    if (FcObjectName (e2->object)) {
		fprintf (stream, "\t%s: (None) -> ", FcObjectName (e2->object));
		FcValueListPrintFile (stream, FcPatternEltValues (e2));
		fprintf (stream, "\n");
	    }
	}
    }
    if (p1 != pp1)
	FcPatternDestroy (p1);
    if (p2 != pp2)
	FcPatternDestroy (p2);
}

void
FcPatternPrint2 (FcPattern         *pp1,
                 FcPattern         *pp2,
                 const FcObjectSet *os)
{
    FcPatternPrint2File (stdout, pp1, pp2, os);
}

void
FcOpPrintFile (FILE *stream, FcOp op_)
{
    FcOp op = FC_OP_GET_OP (op_);

    switch (op) {
    case FcOpInteger: fprintf (stream, "Integer"); break;
    case FcOpDouble: fprintf (stream, "Double"); break;
    case FcOpString: fprintf (stream, "String"); break;
    case FcOpMatrix: fprintf (stream, "Matrix"); break;
    case FcOpRange: fprintf (stream, "Range"); break;
    case FcOpBool: fprintf (stream, "Bool"); break;
    case FcOpCharSet: fprintf (stream, "CharSet"); break;
    case FcOpLangSet: fprintf (stream, "LangSet"); break;
    case FcOpField: fprintf (stream, "Field"); break;
    case FcOpConst: fprintf (stream, "Const"); break;
    case FcOpAssign: fprintf (stream, "Assign"); break;
    case FcOpAssignReplace: fprintf (stream, "AssignReplace"); break;
    case FcOpPrepend: fprintf (stream, "Prepend"); break;
    case FcOpPrependFirst: fprintf (stream, "PrependFirst"); break;
    case FcOpAppend: fprintf (stream, "Append"); break;
    case FcOpAppendLast: fprintf (stream, "AppendLast"); break;
    case FcOpDelete: fprintf (stream, "Delete"); break;
    case FcOpDeleteAll: fprintf (stream, "DeleteAll"); break;
    case FcOpQuest: fprintf (stream, "Quest"); break;
    case FcOpOr: fprintf (stream, "Or"); break;
    case FcOpAnd: fprintf (stream, "And"); break;
    case FcOpEqual:
	fprintf (stream, "Equal");
	FcOpFlagsPrintFile (stream, op_);
	break;
    case FcOpNotEqual:
	fprintf (stream, "NotEqual");
	FcOpFlagsPrintFile (stream, op_);
	break;
    case FcOpLess: fprintf (stream, "Less"); break;
    case FcOpLessEqual: fprintf (stream, "LessEqual"); break;
    case FcOpMore: fprintf (stream, "More"); break;
    case FcOpMoreEqual: fprintf (stream, "MoreEqual"); break;
    case FcOpContains: fprintf (stream, "Contains"); break;
    case FcOpNotContains: fprintf (stream, "NotContains"); break;
    case FcOpPlus: fprintf (stream, "Plus"); break;
    case FcOpMinus: fprintf (stream, "Minus"); break;
    case FcOpTimes: fprintf (stream, "Times"); break;
    case FcOpDivide: fprintf (stream, "Divide"); break;
    case FcOpNot: fprintf (stream, "Not"); break;
    case FcOpNil: fprintf (stream, "Nil"); break;
    case FcOpComma: fprintf (stream, "Comma"); break;
    case FcOpFloor: fprintf (stream, "Floor"); break;
    case FcOpCeil: fprintf (stream, "Ceil"); break;
    case FcOpRound: fprintf (stream, "Round"); break;
    case FcOpTrunc: fprintf (stream, "Trunc"); break;
    case FcOpListing:
	fprintf (stream, "Listing");
	FcOpFlagsPrintFile (stream, op_);
	break;
    case FcOpInvalid: fprintf (stream, "Invalid"); break;
    }
}

void
FcOpPrint (FcOp op_)
{
    FcOpPrintFile (stdout, op_);
}

void
FcExprPrintFile (FILE *stream, const FcExpr *expr)
{
    if (!expr)
	fprintf (stream, "none");
    else
	switch (FC_OP_GET_OP (expr->op)) {
	case FcOpInteger: fprintf (stream, "%d", expr->u.ival); break;
	case FcOpDouble: fprintf (stream, "%g", expr->u.dval); break;
	case FcOpString: fprintf (stream, "\"%s\"", expr->u.sval); break;
	case FcOpMatrix:
	    fprintf (stream, "[");
	    FcExprPrintFile (stream, expr->u.mexpr->xx);
	    fprintf (stream, " ");
	    FcExprPrintFile (stream, expr->u.mexpr->xy);
	    fprintf (stream, "; ");
	    FcExprPrintFile (stream, expr->u.mexpr->yx);
	    fprintf (stream, " ");
	    FcExprPrintFile (stream, expr->u.mexpr->yy);
	    fprintf (stream, "]");
	    break;
	case FcOpRange:
	    fprintf (stream, "(%g, %g)", expr->u.rval->begin, expr->u.rval->end);
	    break;
	case FcOpBool: fprintf (stream, "%s", expr->u.bval ? "true" : "false"); break;
	case FcOpCharSet: fprintf (stream, "charset\n"); break;
	case FcOpLangSet:
	    fprintf (stream, "langset:");
	    FcLangSetPrintFile (stream, expr->u.lval);
	    fprintf (stream, "\n");
	    break;
	case FcOpNil: fprintf (stream, "nil\n"); break;
	case FcOpField:
	    fprintf (stream, "%s ", FcObjectName (expr->u.name.object));
	    switch ((int)expr->u.name.kind) {
	    case FcMatchPattern:
		fprintf (stream, "(pattern) ");
		break;
	    case FcMatchFont:
		fprintf (stream, "(font) ");
		break;
	    }
	    break;
	case FcOpConst: fprintf (stream, "%s", expr->u.constant); break;
	case FcOpQuest:
	    FcExprPrintFile (stream, expr->u.tree.left);
	    fprintf (stream, " quest ");
	    FcExprPrintFile (stream, expr->u.tree.right->u.tree.left);
	    fprintf (stream, " colon ");
	    FcExprPrintFile (stream, expr->u.tree.right->u.tree.right);
	    break;
	case FcOpAssign:
	case FcOpAssignReplace:
	case FcOpPrependFirst:
	case FcOpPrepend:
	case FcOpAppend:
	case FcOpAppendLast:
	case FcOpOr:
	case FcOpAnd:
	case FcOpEqual:
	case FcOpNotEqual:
	case FcOpLess:
	case FcOpLessEqual:
	case FcOpMore:
	case FcOpMoreEqual:
	case FcOpContains:
	case FcOpListing:
	case FcOpNotContains:
	case FcOpPlus:
	case FcOpMinus:
	case FcOpTimes:
	case FcOpDivide:
	case FcOpComma:
	    FcExprPrintFile (stream, expr->u.tree.left);
	    fprintf (stream, " ");
	    switch (FC_OP_GET_OP (expr->op)) {
	    case FcOpAssign: fprintf (stream, "Assign"); break;
	    case FcOpAssignReplace: fprintf (stream, "AssignReplace"); break;
	    case FcOpPrependFirst: fprintf (stream, "PrependFirst"); break;
	    case FcOpPrepend: fprintf (stream, "Prepend"); break;
	    case FcOpAppend: fprintf (stream, "Append"); break;
	    case FcOpAppendLast: fprintf (stream, "AppendLast"); break;
	    case FcOpOr: fprintf (stream, "Or"); break;
	    case FcOpAnd: fprintf (stream, "And"); break;
	    case FcOpEqual:
		fprintf (stream, "Equal");
		FcOpFlagsPrintFile (stream, expr->op);
		break;
	    case FcOpNotEqual:
		fprintf (stream, "NotEqual");
		FcOpFlagsPrintFile (stream, expr->op);
		break;
	    case FcOpLess: fprintf (stream, "Less"); break;
	    case FcOpLessEqual: fprintf (stream, "LessEqual"); break;
	    case FcOpMore: fprintf (stream, "More"); break;
	    case FcOpMoreEqual: fprintf (stream, "MoreEqual"); break;
	    case FcOpContains: fprintf (stream, "Contains"); break;
	    case FcOpListing:
		fprintf (stream, "Listing");
		FcOpFlagsPrintFile (stream, expr->op);
		break;
	    case FcOpNotContains: fprintf (stream, "NotContains"); break;
	    case FcOpPlus: fprintf (stream, "Plus"); break;
	    case FcOpMinus: fprintf (stream, "Minus"); break;
	    case FcOpTimes: fprintf (stream, "Times"); break;
	    case FcOpDivide: fprintf (stream, "Divide"); break;
	    case FcOpComma: fprintf (stream, "Comma"); break;
	    default: break;
	    }
	    fprintf (stream, " ");
	    FcExprPrintFile (stream, expr->u.tree.right);
	    break;
	case FcOpNot:
	    fprintf (stream, "Not ");
	    FcExprPrintFile (stream, expr->u.tree.left);
	    break;
	case FcOpFloor:
	    fprintf (stream, "Floor ");
	    FcExprPrintFile (stream, expr->u.tree.left);
	    break;
	case FcOpCeil:
	    fprintf (stream, "Ceil ");
	    FcExprPrintFile (stream, expr->u.tree.left);
	    break;
	case FcOpRound:
	    fprintf (stream, "Round ");
	    FcExprPrintFile (stream, expr->u.tree.left);
	    break;
	case FcOpTrunc:
	    fprintf (stream, "Trunc ");
	    FcExprPrintFile (stream, expr->u.tree.left);
	    break;
	case FcOpInvalid: fprintf (stream, "Invalid"); break;
	}
}

void
FcExprPrint (const FcExpr *expr)
{
    FcExprPrintFile (stdout, expr);
}

void
FcTestPrintFile (FILE *stream, const FcTest *test)
{
    switch (test->kind) {
    case FcMatchPattern:
	fprintf (stream, "pattern ");
	break;
    case FcMatchFont:
	fprintf (stream, "font ");
	break;
    case FcMatchScan:
	fprintf (stream, "scan ");
	break;
    case FcMatchKindEnd:
	/* shouldn't be reached */
	return;
    }
    switch (test->qual) {
    case FcQualAny:
	fprintf (stream, "any ");
	break;
    case FcQualAll:
	fprintf (stream, "all ");
	break;
    case FcQualFirst:
	fprintf (stream, "first ");
	break;
    case FcQualNotFirst:
	fprintf (stream, "not_first ");
	break;
    }
    fprintf (stream, "%s ", FcObjectName (test->object));
    FcOpPrintFile (stream, test->op);
    fprintf (stream, " ");
    FcExprPrintFile (stream, test->expr);
    fprintf (stream, "\n");
}

void
FcTestPrint (const FcTest *test)
{
    FcTestPrintFile (stdout, test);
}

void
FcEditPrintFile (FILE *stream, const FcEdit *edit)
{
    fprintf (stream, "Edit %s ", FcObjectName (edit->object));
    FcOpPrintFile (stream, edit->op);
    fprintf (stream, " ");
    FcExprPrintFile (stream, edit->expr);
}

void
FcEditPrint (const FcEdit *edit)
{
    FcEditPrintFile (stdout, edit);
}

void
FcRulePrintFile (FILE *stream, const FcRule *rule)
{
    FcRuleType    last_type = FcRuleUnknown;
    const FcRule *r;

    for (r = rule; r; r = r->next) {
	if (last_type != r->type) {
	    switch (r->type) {
	    case FcRuleTest:
		fprintf (stream, "[test]\n");
		break;
	    case FcRuleEdit:
		fprintf (stream, "[edit]\n");
		break;
	    default:
		break;
	    }
	    last_type = r->type;
	}
	fprintf (stream, "\t");
	switch (r->type) {
	case FcRuleTest:
	    FcTestPrintFile (stream, r->u.test);
	    break;
	case FcRuleEdit:
	    FcEditPrintFile (stream, r->u.edit);
	    fprintf (stream, ";\n");
	    break;
	default:
	    break;
	}
    }
    fprintf (stream, "\n");
}

void
FcRulePrint (const FcRule *rule)
{
    FcRulePrintFile (stdout, rule);
}

void
FcFontSetPrintFile (FILE *stream, const FcFontSet *s)
{
    int i;

    fprintf (stream, "FontSet %d of %d\n", s->nfont, s->sfont);
    for (i = 0; i < s->nfont; i++) {
	fprintf (stream, "Font %d ", i);
	FcPatternPrintFile (stream, s->fonts[i]);
    }
}

void
FcFontSetPrint (const FcFontSet *s)
{
    FcFontSetPrintFile (stdout, s);
}

int FcDebugVal;

void
FcInitDebug (void)
{
    if (!FcDebugVal) {
	char *e;

	e = getenv ("FC_DEBUG");
	if (e) {
	    fprintf (stderr, "FC_DEBUG=%s\n", e);
	    FcDebugVal = atoi (e);
	    if (FcDebugVal < 0)
		FcDebugVal = 0;
	}
    }
}
#define __fcdbg__
#include "fcaliastail.h"
#undef __fcdbg__
