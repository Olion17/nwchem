/*
 $Id: nwchem_wrap.c,v 1.38 2007-08-08 16:33:02 d3p852 Exp $
*/
#if defined(DECOSF)
#include <alpha/varargs.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include <Python.h>
#include <abstract.h>

#include "rtdb.h"
#include "macdecls.h"
#include "global.h"
#include "typesf2c.h"

static PyObject *NwchemError;

static Integer rtdb_handle;            /* handle to the rtdb */

#if defined(CRAY_T3E) || defined(WIN32)
#define task_energy_ TASK_ENERGY
#define task_gradient_ TASK_GRADIENT
#define task_optimize_ TASK_OPTIMIZE
#define task_coulomb_ TASK_COULOMB
#define task_coulomb_ref_ TASK_COULOMB_REF
#define task_saddle_ TASK_SADDLE
#define task_freq_ TASK_FREQ
#define task_hessian_ TASK_HESSIAN
#define util_sggo_ UTIL_SGGO
#define util_sgend_ UTIL_SGEND
#define util_sgroup_numgroups_ UTIL_SGROUP_NUMGROUPS
#define util_sgroup_mygroup_ UTIL_SGROUP_MYGROUP
#endif

extern int nw_inp_from_string(int, const char *);

extern Integer FATR task_energy_(const Integer *);
extern Integer FATR task_gradient_(const Integer *);
extern Integer FATR task_optimize_(const Integer *);
extern Integer FATR task_coulomb_(const Integer *);
extern Integer FATR task_coulomb_ref_(const Integer *);
extern Integer FATR task_saddle_(const Integer *);
extern Integer FATR task_freq_(const Integer *);
extern Integer FATR task_hessian_(const Integer *);
extern void FATR util_sggo_(const Integer *, const Integer *);
extern void FATR util_sgend_(const Integer *);
extern Integer FATR util_sgroup_numgroups_(void);
extern Integer FATR util_sgroup_mygroup_(void);

static PyObject *nwwrap_integers(int n, Integer a[])
{
  PyObject *s;
  int i;

  if (n == 1)
    return PyInt_FromLong(a[0]);

  if (!(s=PyList_New(n))) return NULL;
  for(i=0; i<n; i++) {
    PyObject *o = PyInt_FromLong(a[i]);
    if (!o) {
      Py_DECREF(s);
      return NULL;
    }
    if (PyList_SetItem(s,i,o)) {
      Py_DECREF(s);
      return NULL;
    }
  }
  return s;
}

static PyObject *nwwrap_doubles(int n, double a[])
{
  PyObject *s;
  int i;

  if (n == 1)
    return PyFloat_FromDouble(a[0]);

  if (!(s=PyList_New(n))) return NULL;
  for(i=0; i<n; i++) {
    PyObject *o = PyFloat_FromDouble(a[i]);
    if (!o) {
      Py_DECREF(s);
      return NULL;
    }
    if (PyList_SetItem(s,i,o)) {
      Py_DECREF(s);
      return NULL;
    }
  }
  return s;
}

static PyObject *nwwrap_strings(int n, char *a[])
{
  PyObject *s;
  int i;

  if (n == 1)
    return PyString_FromString(a[0]);

  if (!(s=PyList_New(n))) return NULL;
  for(i=0; i<n; i++) {
    PyObject *o = PyString_FromString(a[i]);
    if (!o) {
      Py_DECREF(s);
      return NULL;
    }
    if (PyList_SetItem(s,i,o)) {
      Py_DECREF(s);
      return NULL;
    }
  }
  return s;
}


static PyObject *wrap_rtdb_open(PyObject *self, PyObject *args)
{
   const char *filename, *mode;
   int inthandle;

   if (PyArg_Parse(args, "(ss)", &filename, &mode)) {
       if (!rtdb_open(filename, mode, &inthandle)) {
           PyErr_SetString(NwchemError, "rtdb_open failed");
           return NULL;
       }
       rtdb_handle = inthandle;
   }
   else {
      PyErr_SetString(PyExc_TypeError, "Usage: rtdb_open(filename, mode)");
      return NULL;
   }
   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *wrap_rtdb_close(PyObject *self, PyObject *args)
{
   const char *mode;
   int  result;

   if (PyArg_Parse(args, "s", &mode)) {
       if (!(result = rtdb_close(rtdb_handle, mode))) {
           PyErr_SetString(NwchemError, "rtdb_close failed");
           return NULL;
       }
   }
   else {
       PyErr_SetString(PyExc_TypeError, "Usage: rtdb_close(mode)");
       return NULL;
   }
   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *wrap_pass_handle(PyObject *self, PyObject *args)
{
  int inthandle;
   if (!(PyArg_Parse(args, "i", &inthandle))) {
      PyErr_SetString(PyExc_TypeError, "Usage: pass_handle(rtdb_handle)");
      return NULL;
   }
   rtdb_handle = inthandle;
   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *wrap_rtdb_print(PyObject *self, PyObject *args)
{
   int flag;

   if (PyArg_Parse(args, "i", &flag)) {
      if (!rtdb_print(rtdb_handle, flag)) 
           PyErr_SetString(NwchemError, "rtdb_print failed");
   }
   else {
      PyErr_SetString(PyExc_TypeError, "Usage: rtdb_print(flag)");
      return NULL;
   }
   Py_INCREF(Py_None);
   return Py_None;
}


static PyObject *wrap_rtdb_put(PyObject *self, PyObject *args)
{
    int i, list, list_len;
    int ma_type = -1;
    char *name;
    Integer* int_array;
    double *dbl_array;
    char *char_array;
    char cbuf[8192], *ptr;
    void *array = 0;
    PyObject *obj, *option_obj;

    if ((PyTuple_Size(args) == 2) || (PyTuple_Size(args) == 3)) {
      obj = PyTuple_GetItem(args, 0);      /* get var name */
      PyArg_Parse(obj, "s", &name);
      obj = PyTuple_GetItem(args, 1);      /* get an array or single value */
      
      if (PyList_Check(obj)) 
        list = 1; 
      else 
        list = 0;
      
      if (list) {
        list_len = PyList_Size(obj);
        if (   PyInt_Check(PyList_GetItem(obj, 0)))  
          ma_type = MT_F_INT;
        else if ( PyFloat_Check(PyList_GetItem(obj, 0)))  
          ma_type = MT_F_DBL;
        else if (PyString_Check(PyList_GetItem(obj, 0))) 
          ma_type = MT_CHAR;
        else {
          printf("ERROR A\n");
          ma_type = -1;
        }
      } else {
        list_len = 1;
        if (   PyInt_Check(obj))  
          ma_type = MT_F_INT;
        else if ( PyFloat_Check(obj))  
          ma_type = MT_F_DBL;
        else if (PyString_Check(obj))  
          ma_type = MT_CHAR; 
        else {
          printf("ERROR B\n");
          ma_type = -1;
        }
      } 

      if (ma_type == -1) {
          PyErr_SetString(PyExc_TypeError, 
                          "Usage: rtdb_put - ma_type is confused");
          return NULL;
      }
      
      if (PyTuple_Size(args) == 3) {
        int intma_type;
        option_obj = PyTuple_GetItem(args, 2);      /* get optional type */
        if (!(PyArg_Parse(option_obj, "i", &intma_type))) {
          PyErr_SetString(PyExc_TypeError, 
                          "Usage: rtdb_put(value or values,[optional type])");
          return NULL;
        }
        ma_type = intma_type;
      }
      
      if (ma_type != MT_CHAR) {
        if (!(array = malloc(MA_sizeof(ma_type, list_len, MT_CHAR)))) {
          PyErr_SetString(PyExc_MemoryError,
                          "rtdb_put failed allocating work array");
          return NULL;
        }
      }
      
      switch (ma_type) {
      case MT_INT:
      case MT_F_INT:  
      case MT_BASE + 11:        /* Logical */
        int_array = array;
        for (i = 0; i < list_len; i++) {
          if (list) 
            int_array[i] = PyInt_AS_LONG(PyList_GetItem(obj, i));
          else 
            int_array[i] = PyInt_AS_LONG(obj);
        }
        break;
        
      case MT_DBL:  
      case MT_F_DBL:
        dbl_array = array;
        for (i = 0; i < list_len; i++) {
          if (list) 
            PyArg_Parse(PyList_GetItem(obj, i), "d", dbl_array+i);
          else 
            PyArg_Parse(obj, "d", dbl_array+i);
        }
        break;
        
      case MT_CHAR: 
        ptr = cbuf;
        *ptr = 0;
        for (i = 0; i < list_len; i++) {
          if (list) 
            PyArg_Parse(PyList_GetItem(obj, i), "s", &char_array); 
          else 
            PyArg_Parse(obj, "s", &char_array); 
          /*printf("PROCESSED '%s'\n", char_array);*/
          if ((ptr+strlen(char_array)) >= (cbuf+sizeof(cbuf))) {
             PyErr_SetString(PyExc_MemoryError,"rtdb_put too many strings");
             return NULL;
           }
          strcpy(ptr,char_array);
          ptr = ptr+strlen(char_array);
          strcpy(ptr,"\n");
          ptr = ptr + 1;
        }                
        list_len = strlen(cbuf) + 1;
        array = cbuf;
        break;
        
      default:
        PyErr_SetString(NwchemError, "rtdb_put: ma_type is incorrect");
        if (array) free(array);
        return NULL;
        break;
      }                
      
      if (!(rtdb_put(rtdb_handle, name, ma_type, list_len, array))) {
        PyErr_SetString(NwchemError, "rtdb_put failed");
        if ((ma_type != MT_CHAR) && array) free(array);
        return NULL;
      }
      
    } else {
      PyErr_SetString(PyExc_TypeError, 
                      "Usage: rtdb_put(value or values,[optional type])");
      if ((ma_type != MT_CHAR) && array) free(array);
      return NULL;
    }
    Py_INCREF(Py_None);
    if ((ma_type != MT_CHAR) && array) free(array);
    return Py_None;
}

PyObject *wrap_rtdb_get(PyObject *self, PyObject *args)
{
  int nelem, ma_type;
  char *name;
#define MAXPTRS 2048
  char *ptrs[MAXPTRS];
  PyObject *returnObj = 0;
  char format_char;
  void *array=0;
  int ma_handle;
  
  if (PyArg_Parse(args, "s", &name)) {
    if (!rtdb_ma_get(rtdb_handle, name, &ma_type, &nelem, &ma_handle)) {
      PyErr_SetString(NwchemError, "rtdb_ma_get failed");
      return NULL;
    }
    if (!MA_get_pointer(ma_handle, &array)) {
      PyErr_SetString(NwchemError, "rtdb_ma_get failed");
      return NULL;
    }
    /*printf("name=%s ma_type=%d nelem=%d ptr=%x\n",name, ma_type, 
      nelem, array);*/
    
    switch (ma_type) {
    case MT_F_INT:
    case MT_INT  : 
    case MT_BASE + 11  : 
      format_char = 'i'; break;
    case MT_F_DBL: 
    case MT_DBL  : 
      format_char = 'd'; break;
      break;
    case MT_CHAR : 
      format_char = 's'; break;
    default:
      PyErr_SetString(NwchemError, "rtdb_get: ma type incorrect");
      (void) MA_free_heap(ma_handle);
      return NULL;
      break;
    }
    
    /* For character string need to build an array of pointers */
    
    if (ma_type == MT_CHAR) {
      char *ptr, *next;
      nelem = 0;
      next = ptr = array;
      while (1) {
        int eos = (*ptr == 0);
        if ((*ptr == '\n') || (*ptr == 0)) {
          *ptr = 0;
          if (nelem >= MAXPTRS) {
            PyErr_SetString(PyExc_MemoryError,"rtdb_get too many strings");
            (void) MA_free_heap(ma_handle);
            return NULL;
          }
          if (strlen(next) > 0) {
            ptrs[nelem] = next;
            nelem++;
          }
          next = ptr+1;
          if (eos) break;
        }
        ptr++;
      }
    }
           
    switch (format_char) {
    case 'i':
      returnObj = nwwrap_integers(nelem, array); break;
    case 'd':
      returnObj = nwwrap_doubles(nelem, array); break;
    case 's':
      returnObj = nwwrap_strings(nelem, ptrs); break;
    }

    (void) MA_free_heap(ma_handle);

    if (!returnObj) {
      PyErr_SetString(PyExc_TypeError, "rtdb_get: conversion to python object failed.");
    }
  }
  else {
    PyErr_SetString(PyExc_TypeError, "Usage: value = rtdb_get(name)");
  }

  return returnObj;
}

PyObject *wrap_rtdb_delete(PyObject *self, PyObject *args)
{
   char *name;
   PyObject *returnObj = NULL;

   if (PyArg_Parse(args, "s", &name)) {
       if (rtdb_delete(rtdb_handle, name)) {
         returnObj = Py_None;
         Py_INCREF(Py_None);
       }
       else {
           PyErr_SetString(NwchemError, "rtdb_delete failed");
       }
   }
   else {
       PyErr_SetString(PyExc_TypeError, "Usage: value = rtdb_delete(name)");
   }
   return returnObj;
}

PyObject *wrap_rtdb_get_info(PyObject *self, PyObject *args)
{
   int nelem, ma_type;
   char *name;
   PyObject *returnObj = 0;
   char date[26];

   if (PyArg_Parse(args, "s", &name)) {
       if (!rtdb_get_info(rtdb_handle, name, &ma_type, &nelem, date)) {
           PyErr_SetString(NwchemError, "rtdb_get_info failed");
           return NULL;
       }
       if (!(returnObj = PyTuple_New(3))) {
           PyErr_SetString(NwchemError, "rtdb_get_info failed with pyobj");
           return NULL;
       }
       PyTuple_SET_ITEM(returnObj, 0, PyInt_FromLong((long) ma_type)); 
       PyTuple_SET_ITEM(returnObj, 1, PyInt_FromLong((long) nelem)); 
       PyTuple_SET_ITEM(returnObj, 2, PyString_FromString(date)); 
   }
   else {
       PyErr_SetString(PyExc_TypeError, "Usage: value = rtdb_get_info(name)");
       return NULL;
   }
   return returnObj;
}


PyObject *wrap_rtdb_first(PyObject *self, PyObject *args)
{
   char name[256];
   PyObject *returnObj = NULL;

   if (rtdb_first(rtdb_handle, sizeof(name), name)) {
     returnObj = PyString_FromString(name); /*Py_BuildValue("s#", name, 1); */
   }
   else {
       PyErr_SetString(NwchemError, "rtdb_first: failed");
       return NULL;
   }
   return returnObj;
}

PyObject *wrap_rtdb_next(PyObject *self, PyObject *args)
{
   char name[256];
   PyObject *returnObj = NULL;

   if (rtdb_next(rtdb_handle, sizeof(name), name)) {
     returnObj = PyString_FromString(name); /*Py_BuildValue("s#", name, 1); */
   }
   else {
       PyErr_SetString(NwchemError, "rtdb_next: failed");
       return NULL;
   }
   return returnObj;
}

static PyObject *wrap_task_coulomb_ref(PyObject *self, PyObject *args)
{
    char *theory;
    double energy;
    
    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_coulomb_ref: putting theory failed");
            return NULL;
        }
        if (!rtdb_put(rtdb_handle, "scf:scftype", MT_CHAR, 3, "UHF")) {
            PyErr_SetString(NwchemError, "task_coulomb_ref: putting UHF failed");
            return NULL;
        }
        if (!task_energy_(&rtdb_handle)) {
            PyErr_SetString(NwchemError, "task_coulomb_ref: failed");
            return NULL;
        }
        if (!rtdb_get(rtdb_handle, "uhf:coulomb", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_coulomb_ref: getting coulomb energy failed");
            return NULL;
        }
/*      printf("Coulomb ref: %f",energy); */
        if (!rtdb_put(rtdb_handle, "uhf:coulombref", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_coulomb_ref: putting reference energy failed");
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_coulomb_ref(theory)");
        return NULL;
    }
    
    return Py_BuildValue("d", energy);
}
static PyObject *wrap_task_coulomb(PyObject *self, PyObject *args)
{
    char *theory;
    double energy, refenergy;
    
    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_coulomb: putting theory failed");
            return NULL;
        }
        if (!rtdb_put(rtdb_handle, "scf:scftype", MT_CHAR, 3, "UHF")) {
            PyErr_SetString(NwchemError, "task_coulomb_ref: putting UHF failed");
            return NULL;
        }
        if (!task_energy_(&rtdb_handle)) {} /*{
            PyErr_SetString(NwchemError, "task_coulomb: failed");
            return NULL;
        } */
        if (!rtdb_get(rtdb_handle, "uhf:coulomb", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_coulomb: getting coulomb energy failed");
            return NULL;
        }
        if (!rtdb_get(rtdb_handle, "uhf:coulombref", MT_F_DBL, 1, &refenergy)) {
            PyErr_SetString(NwchemError, "task_coulomb: getting coulomb ref energy failed");
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_coulomb(theory)");
        return NULL;
    }
    
/*  printf("\n\n Reference energy = %f   Energy = %f         Error = %f \n\n",refenergy,energy,fabs(refenergy-energy)); */
    return Py_BuildValue("d", fabs(refenergy-energy));
}

static PyObject *wrap_task_energy(PyObject *self, PyObject *args)
{
    char *theory;
    double energy;
    
    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_energy: putting theory failed");
            return NULL;
        }
        if (!task_energy_(&rtdb_handle)) {
            PyErr_SetString(NwchemError, "task_energy: failed");
            return NULL;
        }
        if (!rtdb_get(rtdb_handle, "task:energy", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_energy: getting energy failed");
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_energy(theory)");
        return NULL;
    }
    
    return Py_BuildValue("d", energy);
}

static PyObject *wrap_task_gradient(PyObject *self, PyObject *args)
{
    char *theory;
    double energy, *gradient;
    int ma_type, nelem, ma_handle;
    PyObject *returnObj, *eObj, *gradObj;
    
    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_gradient: putting theory failed");
            return NULL;
        }
        if (!task_gradient_(&rtdb_handle)) {
            PyErr_SetString(NwchemError, "task_gradient: failed");
            return NULL;
        }
        if (!rtdb_get(rtdb_handle, "task:energy", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_gradient: getting energy failed");
            return NULL;
        }
        if (!rtdb_ma_get(rtdb_handle,"task:gradient",&ma_type,&nelem,&ma_handle)) {
            PyErr_SetString(NwchemError, "task_gradient: getting gradient failed");
            return NULL;
        }
        if (!MA_get_pointer(ma_handle, &gradient)) {
            PyErr_SetString(NwchemError, "task_gradient: ma_get_ptr failed");
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_gradient(theory)");
        return NULL;
    }
    
    eObj = Py_BuildValue("d",energy);
    gradObj = nwwrap_doubles(nelem, gradient);
    returnObj = Py_BuildValue("OO", eObj, gradObj);
    Py_DECREF(eObj);
    Py_DECREF(gradObj);
    (void) MA_free_heap(ma_handle);

    return returnObj;
}

static PyObject *wrap_task_optimize(PyObject *self, PyObject *args)
{
    char *theory;
    double energy, *gradient;
    int ma_type, nelem, ma_handle;
    PyObject *returnObj, *eObj, *gradObj;
    
    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_optimize: putting theory failed");
            return NULL;
        }
        if (!task_optimize_(&rtdb_handle)) {
            PyErr_SetString(NwchemError, "task_optimize: failed");
            return NULL;
        }
        if (!rtdb_get(rtdb_handle, "task:energy", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_optimize: getting energy failed");
            return NULL;
        }
        if (!rtdb_ma_get(rtdb_handle,"task:gradient",&ma_type,&nelem,&ma_handle)) {
            PyErr_SetString(NwchemError, "task_optimize: getting gradient failed");
            return NULL;
        }
        if (!MA_get_pointer(ma_handle, &gradient)) {
            PyErr_SetString(NwchemError, "task_optimize: ma_get_ptr failed");
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_optimize(theory)");
        return NULL;
    }
    
    eObj = Py_BuildValue("d",energy);
    gradObj = nwwrap_doubles(nelem, gradient);
    returnObj = Py_BuildValue("OO", eObj, gradObj);
    Py_DECREF(eObj);
    Py_DECREF(gradObj);
    (void) MA_free_heap(ma_handle);

    return returnObj;
}


static PyObject *wrap_task_hessian(PyObject *self, PyObject *args)
{
    char *theory;

    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_hessian: putting theory failed");

            return NULL;
        }
        if (!task_hessian_(&rtdb_handle)){
            PyErr_SetString(NwchemError, "task_hessian: failed");
            return NULL;
        }
    }   
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_hessian(theory)");
        return NULL;
    }

    return NULL;
}

static PyObject *wrap_task_freq(PyObject *self, PyObject *args)
{
    char *theory;
    double zpe, *frequencies, *intensities;
    int ma_type, nelem, ma_handle;
    PyObject *returnObj, *zpeObj, *freqObj, *intObj;


    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_freq: putting theory failed");
            
            return NULL;
        }

    if (!task_freq_(&rtdb_handle)) {
            PyErr_SetString(NwchemError, "task_freq: failed");

            return NULL;
        }

        if (!rtdb_get(rtdb_handle, "vib:zpe", MT_F_DBL, 1, &zpe)) {
            PyErr_SetString(NwchemError, "task_freq: getting energy failed");

            return NULL;
        }
    if (!rtdb_ma_get(rtdb_handle,"vib:projected frequencies",&ma_type,&nelem,&ma_handle)){

            PyErr_SetString(NwchemError, "task_freq: getting frequencies failed");

            return NULL;
        }
    if (!MA_get_pointer(ma_handle, &frequencies)) {
            PyErr_SetString(NwchemError, "task_freq: ma_get_ptr failed");
            return NULL;
        }

    if (!rtdb_ma_get(rtdb_handle,"vib:projected intensities",&ma_type,&nelem,&ma_handle)){

            PyErr_SetString(NwchemError, "task_freq: getting frequencies failed");

            return NULL;
        }
    if (!MA_get_pointer(ma_handle, &intensities)) {
            PyErr_SetString(NwchemError, "task_freq: ma_get_ptr failed");
            return NULL;
        }
    }

    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_freq(theory)");
        return NULL;
    }

    zpeObj = Py_BuildValue("d",zpe);
    freqObj = nwwrap_doubles(nelem, frequencies);
    intObj = nwwrap_doubles(nelem, intensities);
    returnObj = Py_BuildValue("OOO", zpeObj, freqObj, intObj);
    Py_DECREF(zpeObj);
    Py_DECREF(freqObj);
    Py_DECREF(intObj);
    (void) MA_free_heap(ma_handle);

    return returnObj;
}



static PyObject *wrap_task_saddle(PyObject *self, PyObject *args)
{
    char *theory;
    double energy, *gradient;
    int ma_type, nelem, ma_handle;
    PyObject *returnObj, *eObj, *gradObj;

    if (PyArg_Parse(args, "s", &theory)) {
        if (!rtdb_put(rtdb_handle, "task:theory", MT_CHAR, 
                      strlen(theory)+1, theory)) {
            PyErr_SetString(NwchemError, "task_saddle: putting theory failed");

            return NULL;
        }
    if (!task_saddle_(&rtdb_handle)) {
            PyErr_SetString(NwchemError, "task_saddle: failed");
            return NULL;
        }
        if (!rtdb_get(rtdb_handle, "task:energy", MT_F_DBL, 1, &energy)) {
            PyErr_SetString(NwchemError, "task_saddle: getting energy failed");

            return NULL;
        }
        if (!rtdb_ma_get(rtdb_handle,"task:gradient",&ma_type,&nelem,&ma_handle)){

            PyErr_SetString(NwchemError, "task_saddle: getting gradient failed");

            return NULL;
        }
        if (!MA_get_pointer(ma_handle, &gradient)) {
            PyErr_SetString(NwchemError, "task_saddle: ma_get_ptr failed");
            return NULL;
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Usage: task_saddle(theory)");
        return NULL;
    }
    
    eObj = Py_BuildValue("d",energy);
    gradObj = nwwrap_doubles(nelem, gradient);
    returnObj = Py_BuildValue("OO", eObj, gradObj);
    Py_DECREF(eObj);
    Py_DECREF(gradObj);
    (void) MA_free_heap(ma_handle);

    return returnObj;
}

static PyObject *wrap_nw_inp_from_string(PyObject *self, PyObject *args)
{
   char *pchar;

   if (PyArg_Parse(args, "s", &pchar)) {
       if (!nw_inp_from_string(rtdb_handle, pchar)) {
           PyErr_SetString(NwchemError, "input_parse failed");
           return NULL;
      }
   }
   else {
      PyErr_SetString(PyExc_TypeError, "Usage: input_parse(string)");
      return NULL;
   }
   Py_INCREF(Py_None);
   return Py_None;
}

////  PGroup python routines follow //////////
////  If you have not done any group work, then these are global

/** TODO: 
Allow array of numbers to define the group
Allow array of arrays to define what nodes go where in the group
**/
static PyObject *do_pgroup_create(PyObject *self, PyObject *args)
{
   ///  This routines splits the current group up into subgroups
   Integer num_groups;
   int input;
   PyObject *returnObj;
   // Things returned as a tuple of five items
   Integer mygroup ; // My group number (they are 1 to ngroups)
   Integer ngroups ; // Number of groups at this level
   int nodeid ;      // Node ID in this group (they are 0 to nnodes-1)
   int nnodes ;      // Number of nodes in this group
   Integer my_ga_group ; // The Global Arrays group ID - useful for debug only at this time

   if (!PyArg_Parse(args, "i", &input)) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_create(integer)");
      return NULL;
   }
   num_groups = input;

#ifdef USE_SUBGROUPS
   util_sggo_(&rtdb_handle,&num_groups);
   if (!(returnObj = PyTuple_New(5))) {
       PyErr_SetString(NwchemError, "do_pgroup_create failed with pyobj");
       return NULL;
   }
   my_ga_group = ga_pgroup_get_default_() ;
   nnodes = ga_pgroup_nnodes_(&my_ga_group);
   nodeid = ga_pgroup_nodeid_(&my_ga_group);
   ngroups = util_sgroup_numgroups_() ;
   mygroup = util_sgroup_mygroup_() ;

   PyTuple_SET_ITEM(returnObj, 0, PyInt_FromLong((long) mygroup ));
   PyTuple_SET_ITEM(returnObj, 1, PyInt_FromLong((long) ngroups));
   PyTuple_SET_ITEM(returnObj, 2, PyInt_FromLong((long) nodeid));
   PyTuple_SET_ITEM(returnObj, 3, PyInt_FromLong((long) nnodes));
   PyTuple_SET_ITEM(returnObj, 4, PyInt_FromLong((long) my_ga_group));
   return returnObj ;
#else
   PyErr_SetString(PyExc_TypeError, "Usage: NOT IMPLEMENTED YET");
   return NULL;
#endif
}


static PyObject *do_pgroup_destroy(PyObject *self, PyObject *args)
{
   /// This merges the subgroups back into their former bigger group
   PyObject *returnObj;
   // Things returned as a tuple of five items
   Integer mygroup ; // My group number (they are 1 to ngroups)
   Integer ngroups ; // Number of groups at this level
   int nodeid ;      // Node ID in this group (they are 0 to nnodes-1)
   int nnodes ;      // Number of nodes in this group
   Integer my_ga_group ; // The Global Arrays group ID - useful for debug only at this time
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_destroy()");
      return NULL;
   }
#ifdef USE_SUBGROUPS
   util_sgend_(&rtdb_handle);
   my_ga_group = ga_pgroup_get_default_() ;
   nnodes = ga_pgroup_nnodes_(&my_ga_group);
   nodeid = ga_pgroup_nodeid_(&my_ga_group);
   ngroups = util_sgroup_numgroups_() ;
   mygroup = util_sgroup_mygroup_() ;

   if (!(returnObj = PyTuple_New(5))) {
       PyErr_SetString(NwchemError, "do_pgroup_destroy failed with pyobj");
       return NULL;
   }
   PyTuple_SET_ITEM(returnObj, 0, PyInt_FromLong((long) mygroup ));
   PyTuple_SET_ITEM(returnObj, 1, PyInt_FromLong((long) ngroups));
   PyTuple_SET_ITEM(returnObj, 2, PyInt_FromLong((long) nodeid));
   PyTuple_SET_ITEM(returnObj, 3, PyInt_FromLong((long) nnodes));
   PyTuple_SET_ITEM(returnObj, 4, PyInt_FromLong((long) my_ga_group));
   return returnObj ;
#else
   PyErr_SetString(PyExc_TypeError, "Usage: NOT IMPLEMENTED YET");
   return NULL;
#endif
}

   ///  This is a generic barrier that forces all members of a group to 
   ///  sync up before moving on
static PyObject *do_pgroup_sync_work(PyObject *args, Integer my_group)
{
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_sync() or pgroup_sync_all()");
      return NULL;
   }
   ga_pgroup_sync_(&my_group);
   Py_INCREF(Py_None);
   return Py_None;
}
static PyObject *do_pgroup_sync(PyObject *self, PyObject *args)
{
   Integer my_group = ga_pgroup_get_default_() ;
   PyObject *returnObj = do_pgroup_sync_work(args,my_group);
   return returnObj ;
}
static PyObject *do_pgroup_sync_all(PyObject *self, PyObject *args)
{
   Integer my_group = ga_pgroup_get_world_() ;
   PyObject *returnObj = do_pgroup_sync_work(args,my_group);
   return returnObj ;
}


    ///  This is like the MPI DGOP/IGOP commands.  The determination
    ///  of int vs. double is done based upon all elements, and
    ///  all must be the same on all nodes.
    ///  If no operation is included then "+" is assumed.
static PyObject *do_pgroup_global_op_work(PyObject *args, Integer my_group)
{
    int is_double = 0 ;
    int is_int = 0;
    int is_double_array = 0;
    int i,list,size;
    int tmp_int = 0;
    double tmp_double = 0;
    Integer nelem ;
    PyObject *obj, *returnObj;
    char *pchar;

    size = PyTuple_Size(args);
    if (size <  1) {
       obj  = args;
       *pchar = '+';
    }
    else if (size ==  1) {
       obj  = PyTuple_GetItem(args, 0);
    }
    else {
       obj = PyTuple_GetItem(args, 1);
       if (!PyArg_Parse(obj, "s", &pchar)) *pchar = '+';
       obj  = PyTuple_GetItem(args, 0);
    }

    if (PyList_Check(obj)){
      list = 1;
      nelem = PyList_Size(obj);
    }
    else{
      list = 0;
      nelem = 1;
    }

    for (i = 0; i < nelem; i++) {
       if (list) {
         is_double = PyFloat_Check(PyList_GetItem(obj, i));
         is_int    = PyInt_Check(PyList_GetItem(obj, i));
       } else {
         is_double = PyFloat_Check(obj);
         is_int    = PyInt_Check(obj);
       }
       if (!is_double && !is_int) {
           PyErr_SetString(PyExc_TypeError,
                          "global_op() found non-numerical value");
           return NULL;
       }
       if (!is_int) {
         is_double_array++;
       }
    }

    if (is_double_array > 0) { // Has at least one double
      double *array = 0;
      Integer message_id = 10 ;
      if (!(array = malloc(MA_sizeof(MT_F_DBL, nelem, MT_CHAR)))) {
            PyErr_SetString(PyExc_MemoryError,
                            "pgroup_global_op() failed allocating work array");
         return NULL;
      }

      for (i = 0; i < nelem; i++) {
         if (list) {
           is_int    = PyInt_Check(PyList_GetItem(obj, i));
         } else {
           is_int    = PyInt_Check(obj);
         }
         if (!is_int) {
           if (list) {
             PyArg_Parse(PyList_GetItem(obj, i), "d", array+i);
           } else {
             PyArg_Parse(obj, "d", array+i);
           }
         } else {
           if (list) {
             PyArg_Parse(PyList_GetItem(obj, i), "i", &tmp_int);
           } else {
             PyArg_Parse(obj, "i", &tmp_int);
           }
           array[i] = (double) tmp_int;
         }
      }

      ga_pgroup_dgop_(&my_group,&message_id,array,&nelem,pchar);

      returnObj =  nwwrap_doubles(nelem, array);
      free(array);
    }
    else { // Pure integer array
      Integer *array = 0;
      Integer message_id = 11 ;
      if (!(array = malloc(MA_sizeof(MT_F_INT, nelem, MT_CHAR)))) {
            PyErr_SetString(PyExc_MemoryError,
                            "pgroup_global_op() failed allocating work array");
         return NULL;
      }
      
      for (i = 0; i < nelem; i++) {
         if (list) {
           PyArg_Parse(PyList_GetItem(obj, i), "i", array+i);
         } else {
           PyArg_Parse(obj, "i", array+i);
         }
      }
      
      ga_pgroup_igop_(&my_group,&message_id,array,&nelem,pchar);
      
      returnObj =  nwwrap_integers(nelem, array);
      free(array);
    }
    return returnObj;
}
static PyObject *do_pgroup_global_op(PyObject *self, PyObject *args)
{          
   Integer my_group = ga_pgroup_get_default_() ;
   PyObject *returnObj = do_pgroup_global_op_work(args,my_group);
   return returnObj ;
}      
static PyObject *do_pgroup_global_op_all(PyObject *self, PyObject *args)
{
   Integer my_group = ga_pgroup_get_world_() ;
   PyObject *returnObj = do_pgroup_global_op_work(args,my_group);
   return returnObj ;
}


    ///  This is like the MPI brdcst command.  The determination
    ///  of int vs. double is done based upon the whole array.
    ///  Node zero of the group always does the talking.
    ///  All nodes must have same size object
static PyObject *do_pgroup_broadcast_work(PyObject *args, Integer my_group)
{
    Integer node0 = 0 ;
    int is_double = 0 ;
    int is_int = 0;
    int is_double_array = 0;
    int i,list;
    Integer size;
    int tmp_int = 0;
    double tmp_double = 0;
    Integer nelem ;
    PyObject *obj, *returnObj;

    obj  = args;

    if (PyList_Check(obj)){
      list = 1;
      nelem = PyList_Size(obj);
    }
    else{
      list = 0;
      nelem = 1;
    }

    for (i = 0; i < nelem; i++) {
       if (list) {
         is_double = PyFloat_Check(PyList_GetItem(obj, i));
         is_int    = PyInt_Check(PyList_GetItem(obj, i));
       } else {
         is_double = PyFloat_Check(obj);
         is_int    = PyInt_Check(obj);
       }
       if (!is_double && !is_int) {
           PyErr_SetString(PyExc_TypeError,"global_broadcast() found non-numerical value");
           return NULL;
       }
       if (!is_int) {
         is_double_array++;
       }
    }

    if (is_double_array > 0) { // Has at least one double
      double *array = 0;
      Integer message_id = 12 ;
      size = MA_sizeof(MT_F_DBL, nelem, MT_CHAR) ;
      if (!(array = malloc(size))) {
            PyErr_SetString(PyExc_MemoryError,"pgroup_broadcast() failed allocating work array");
         return NULL;
      }

      for (i = 0; i < nelem; i++) {
         if (list) {
           is_int    = PyInt_Check(PyList_GetItem(obj, i));
         } else {
           is_int    = PyInt_Check(obj);
         }
         if (!is_int) {
           if (list) {
             PyArg_Parse(PyList_GetItem(obj, i), "d", array+i);
           } else {
             PyArg_Parse(obj, "d", array+i);
           }
         } else {
           if (list) {
             PyArg_Parse(PyList_GetItem(obj, i), "i", &tmp_int);
           } else {
             PyArg_Parse(obj, "i", &tmp_int);
           }
           array[i] = (double) tmp_int;
         }
      }

      ga_pgroup_brdcst_(&my_group,&message_id,array,&size,&node0);

      returnObj =  nwwrap_doubles(nelem, array);
      free(array);
    }
    else { // Pure integer array
      Integer *array = 0;
      Integer message_id = 13 ;
      size = MA_sizeof(MT_F_INT, nelem, MT_CHAR) ;
      if (!(array = malloc(size))) {
            PyErr_SetString(PyExc_MemoryError,"pgroup_broadcast() failed allocating work array");
         return NULL;
      }
      
      for (i = 0; i < nelem; i++) {
         if (list) {
           PyArg_Parse(PyList_GetItem(obj, i), "i", array+i);
         } else {
           PyArg_Parse(obj, "i", array+i);
         }
      }
      
      ga_pgroup_brdcst_(&my_group,&message_id,array,&size,&node0);
      
      returnObj =  nwwrap_integers(nelem, array);
      free(array);
    }
    return returnObj;
}
static PyObject *do_pgroup_broadcast(PyObject *self, PyObject *args)
{     
   Integer my_group = ga_pgroup_get_default_() ;
   PyObject *returnObj = do_pgroup_broadcast_work(args,my_group);
   return returnObj ;
}
static PyObject *do_pgroup_broadcast_all(PyObject *self, PyObject *args)
{
   Integer my_group = ga_pgroup_get_world_() ;
   PyObject *returnObj = do_pgroup_broadcast_work(args,my_group);
   return returnObj ;
}

static PyObject *do_pgroup_nnodes(PyObject *self, PyObject *args)
{
   /// Returns the number of nodes in a group
   Integer my_group = ga_pgroup_get_default_() ;
   int nnodes = ga_pgroup_nnodes_(&my_group);
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_nnodes()");
      return NULL;
   }
   return Py_BuildValue("i", nnodes);
}


static PyObject *do_pgroup_nodeid(PyObject *self, PyObject *args)
{
   /// This returns the node number (within the group, not global)
   /// Nodes are numbered, 0 to NumNodes-1
   Integer my_group = ga_pgroup_get_default_() ;
   int nodeid = ga_pgroup_nodeid_(&my_group);
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_nodeid()");
      return NULL;
   }
   return Py_BuildValue("i", nodeid);
}

static PyObject *do_pgroup_ngroups(PyObject *self, PyObject *args)
{
   /// Returns the number of groups at the current groups level
#ifdef USE_SUBGROUPS
   Integer ngroups = util_sgroup_numgroups_() ;
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_ngroups()");
      return NULL;
   }
   return Py_BuildValue("i", ngroups);
#else
   PyErr_SetString(PyExc_TypeError, "Usage: NOT IMPLEMENTED YET");
   return NULL;
#endif
}

static PyObject *do_pgroup_groupid(PyObject *self, PyObject *args)
{
   /// Returns the ID of group at the current groups level 
#ifdef USE_SUBGROUPS
   Integer mygroup = util_sgroup_mygroup_() ;
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: pgroup_groupid()");
      return NULL;
   }
   return Py_BuildValue("i", mygroup);
#else
   PyErr_SetString(PyExc_TypeError, "Usage: NOT IMPLEMENTED YET");
   return NULL;
#endif
}

static PyObject *do_ga_groupid(PyObject *self, PyObject *args)
{
   /// The returns the GA group ID
   Integer my_group = ga_pgroup_get_default_() ;
   if (args) {
      PyErr_SetString(PyExc_TypeError, "Usage: ga_groupid()");
      return NULL;
   }  
   return Py_BuildValue("i", my_group);
}  


/******************************************************************************/
/******************************************************************************/
/******************************************************************************/




static struct PyMethodDef nwchem_methods[] = {
   {"rtdb_open",       wrap_rtdb_open, 0}, 
   {"rtdb_close",      wrap_rtdb_close, 0}, 
   {"pass_handle",     wrap_pass_handle, 0}, 
   {"rtdb_print",      wrap_rtdb_print, 0}, 
   {"rtdb_put",        wrap_rtdb_put, 0}, 
   {"rtdb_get",        wrap_rtdb_get, 0}, 
   {"rtdb_delete",     wrap_rtdb_delete, 0}, 
   {"rtdb_get_info",   wrap_rtdb_get_info, 0}, 
   {"rtdb_first",      wrap_rtdb_first, 0}, 
   {"rtdb_next",       wrap_rtdb_next, 0}, 
   {"task_energy",     wrap_task_energy, 0}, 
   {"task_gradient",   wrap_task_gradient, 0}, 
   {"task_optimize",   wrap_task_optimize, 0}, 
   {"task_hessian",    wrap_task_hessian, 0},
   {"task_saddle",     wrap_task_saddle, 0},
   {"task_freq",       wrap_task_freq, 0},
   {"task_coulomb",    wrap_task_coulomb, 0}, 
   {"task_coulomb_ref",    wrap_task_coulomb_ref, 0}, 
   {"input_parse",     wrap_nw_inp_from_string, 0}, 
   {"ga_nodeid",       do_pgroup_nodeid, 0},  // This is the same as pgroup_nodeid
   {"ga_groupid",      do_ga_groupid, 0},    // This is NOT the same as pgroup_groupid
   {"pgroup_create",   do_pgroup_create, 0},
   {"pgroup_destroy",  do_pgroup_destroy, 0},
   {"pgroup_sync",     do_pgroup_sync, 0},
   {"pgroup_global_op",do_pgroup_global_op, 0},
   {"pgroup_broadcast",do_pgroup_broadcast, 0},
   {"pgroup_sync_all",     do_pgroup_sync_all, 0},
   {"pgroup_global_op_all",do_pgroup_global_op_all, 0},
   {"pgroup_broadcast_all",do_pgroup_broadcast_all, 0},
   {"pgroup_nnodes",   do_pgroup_nnodes, 0},
   {"pgroup_nodeid",   do_pgroup_nodeid, 0},
   {"pgroup_groupid",  do_pgroup_groupid, 0},
   {"pgroup_ngroups",  do_pgroup_ngroups, 0},
   {NULL, NULL}
};

void initnwchem()
{
    PyObject *m, *d;
    m = Py_InitModule("nwchem", nwchem_methods);
    d = PyModule_GetDict(m);
    NwchemError = PyErr_NewException("nwchem.error", NULL, NULL);
    PyDict_SetItemString(d, "NWChemError", NwchemError);
}

