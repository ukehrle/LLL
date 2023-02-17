/*
 * LLL: makes FLINTs LLL implementation available in GAP.
 */

#include <gap_all.h>    // GAP headers
#include <flint/fmpz_lll.h>
#include <stdio.h>


/* both GAP's and FLINT's integer types are essentially GMP integers, but with ways
 * to "directly" store small integers slapped on top of it. We need some boilerplate
 * to translate between these types.
 *
 * TODO: convert directly between "immediate" integers if possible.
 * NOTE: GAP immediate integers have a bit less than FLINTs.
 *   In order to encode whether an integer is small,
 *   - FLINT uses the second most significant bit
 *   - GAP uses the least significant bit *and reserves the second least significant bit*
 *
 * We start with some macros and functions with static linkage shamelessly
 * stolen from <gap>/src/integers.c. If they change anything, this will
 * explode.
 */


#define IS_INTPOS(obj)          (TNUM_OBJ(obj) == T_INTPOS)

typedef struct {
  mpz_t v;
  mp_limb_t tmp;
  Obj obj;
} fake_mpz_t[1];

/****************************************************************************
**
*F  GMPorINTOBJ_MPZ( <fake> )
**
**  This function converts an mpz_t into a GAP integer object.
*/
static Obj GMPorINTOBJ_MPZ( mpz_t v )
{
    return MakeObjInt((const UInt *)v->_mp_d, v->_mp_size);
}

/****************************************************************************
**
**  MPZ_FAKEMPZ( <fake> )
**
**  This converts a fake_mpz_t into an mpz_t. As a side effect, it updates
**  fake->v->_mp_d. This allows us to use SWAP on fake_mpz_t objects, and
**  also protects against garbage collection moving around data.
*/
#define MPZ_FAKEMPZ(fake)   (UPDATE_FAKEMPZ(fake), fake->v)

/* UPDATE_FAKEMPZ is a helper function for the MPZ_FAKEMPZ macro */
static inline void UPDATE_FAKEMPZ( fake_mpz_t fake )
{
  fake->v->_mp_d = fake->obj ? (mp_ptr)ADDR_INT(fake->obj) : &fake->tmp;
}

/****************************************************************************
**
*F  FAKEMPZ_GMPorINTOBJ( <fake>, <op> )
**
**  Initialize <fake> to reference the content of <op>. For this, <op>
**  must be an integer, either small or large, but this is *not* checked.
**  The calling code is responsible for any verification.
*/
static inline void FAKEMPZ_GMPorINTOBJ( fake_mpz_t fake, Obj op )
{
  if (IS_INTOBJ(op)) {
    fake->obj = 0;
    fake->v->_mp_alloc = 1;
    const Int i = INT_INTOBJ(op);
    if ( i >= 0 ) {
      fake->tmp = i;
      fake->v->_mp_size = i ? 1 : 0;
    }
    else {
      fake->tmp = -i;
      fake->v->_mp_size = -1;
    }
  }
  else {
    fake->obj = op;
    fake->v->_mp_alloc = SIZE_INT(op);
    fake->v->_mp_size = IS_INTPOS(op) ? SIZE_INT(op) : -SIZE_INT(op);
  }
}


static void mat_gap2flint(fmpz_mat_t res, const Obj x)
{
    int n, m;
    fake_mpz_t tmp;
    Obj entry;

    n = LEN_LIST(x);
    m = n ? LEN_LIST(ELM_LIST(x, 1)) : 0;

    fmpz_mat_init(res, n, m);

    for (int i=0; i < n; i++) {
        for (int j=0; j < m; j++) {
            entry = ELM_LIST( ELM_LIST(x, i+1), j+1);
            FAKEMPZ_GMPorINTOBJ(tmp, entry);
            fmpz_set_mpz(fmpz_mat_entry(res, i, j), MPZ_FAKEMPZ(tmp));
        }
    }
}

static Obj mat_flint2gap( const fmpz_mat_t x)
{
    int n, m;
    Obj res, row;

    fmpz_t entry;
    mpz_t tmp;

    n = x->r;
    m = x->c;

    mpz_init(tmp);

    res = NEW_PLIST(T_PLIST, n);
    SET_LEN_PLIST(res, n);

    for (int i=1; i <= n; i++) {
        row = NEW_PLIST(T_PLIST, m);
        SET_LEN_PLIST(row, m);
        SET_ELM_PLIST(res, i, row);
        for (int j=1; j <= m; j++) {
            *entry = *fmpz_mat_entry(x, i-1, j-1);
            fmpz_get_mpz(tmp, entry);
            SET_ELM_PLIST(row, j, GMPorINTOBJ_MPZ (tmp));
        }
    }

    mpz_clear(tmp);

    return res;
}

/****************************************************************************
**
*F  LLLReducedGramMatFLINT( <gram>, <delta> )
**
**  Wrapper arount FLINT's LLL algorithm.
**  Gram is a Gram Matrix, delta is the sensitivity as as a GAP MacFloat-object.
*/

Obj FuncLLLReducedGramMatFLINT(Obj self, Obj gram, Obj delta)
{
    Obj res, remainder, transformation;

    fmpz_mat_t mat, trans;
    fmpz_mat_t w_rem, w_trans;
    fmpz_lll_t fl;

    int d;

    fmpz_lll_context_init_default(fl);
    fl->rt = GRAM;
    fl->delta = VAL_MACFLOAT(delta);

    mat_gap2flint(mat, gram);
    fmpz_mat_init(trans, mat->r, mat->r);
    fmpz_mat_one(trans);

    /* do the work */
    fmpz_lll(mat, trans, fl);

    /* strip the zero-part of the gram matrix and remove the
       corresponding rows of the transformation matrix */
    _fmpz_mat_read_only_window_init_strip_initial_zero_rows_and_corresponding_cols(w_rem, mat);
    fmpz_mat_window_init(w_trans, trans, mat->r - w_rem->r, 0, mat->r, mat->r);

    remainder = mat_flint2gap(w_rem);
    transformation = mat_flint2gap(w_trans);

    _fmpz_mat_read_only_window_clear(w_rem);
    _fmpz_mat_read_only_window_clear(w_trans);
    fmpz_mat_clear(mat);
    fmpz_mat_clear(trans);

    res = NEW_PREC(3);
    AssPRec(res, RNamName("remainder"), remainder);
    AssPRec(res, RNamName("transformation"), transformation);

    flint_cleanup_master();

    return res;
}

// Table of functions to export
static StructGVarFunc GVarFuncs [] = {
    GVAR_FUNC(LLLReducedGramMatFLINT, 2, "gram delta"),

    { 0 } /* Finish with an empty entry */
};

/****************************************************************************
**
*F  InitKernel( <module> ) . . . . . . . .  initialise kernel data structures
*/
static Int InitKernel( StructInitInfo *module )
{
    /* init filters and functions */
    InitHdlrFuncsFromTable( GVarFuncs );

    /* return success */
    return 0;
}

/****************************************************************************
**
*F  InitLibrary( <module> ) . . . . . . .  initialise library data structures
*/
static Int InitLibrary( StructInitInfo *module )
{
    /* init filters and functions */
    InitGVarFuncsFromTable( GVarFuncs );

    /* return success */
    return 0;
}

/****************************************************************************
**
*F  Init__Dynamic() . . . . . . . . . . . . . . . . . table of init functions
*/
static StructInitInfo module = {
    .type = MODULE_DYNAMIC,
    .name = "LLL",
    .initKernel = InitKernel,
    .initLibrary = InitLibrary,
};

StructInitInfo *Init__Dynamic( void )
{
    return &module;
}
