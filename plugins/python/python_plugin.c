#include "uwsgi_python.h"

extern struct uwsgi_server uwsgi;
struct uwsgi_python up;

extern struct http_status_codes hsc[];

#include <glob.h>

extern PyTypeObject uwsgi_InputType;

struct option uwsgi_python_options[] = {
	{"wsgi-file", required_argument, 0, LONG_ARGS_WSGI_FILE},
	{"file", required_argument, 0, LONG_ARGS_FILE_CONFIG},
	{"eval", required_argument, 0, LONG_ARGS_EVAL_CONFIG},
	{"module", required_argument, 0, 'w'},
	{"wsgi", required_argument, 0, 'w'},
	{"callable", required_argument, 0, LONG_ARGS_CALLABLE},
	{"test", required_argument, 0, 'J'},
	{"home", required_argument, 0, 'H'},
	{"virtualenv", required_argument, 0, 'H'},
	{"venv", required_argument, 0, 'H'},
	{"pyhome", required_argument, 0, 'H'},
	{"pythonpath", required_argument, 0, LONG_ARGS_PYTHONPATH},
	{"python-path", required_argument, 0, LONG_ARGS_PYTHONPATH},
	{"pymodule-alias", required_argument, 0, LONG_ARGS_PYMODULE_ALIAS},
	{"post-pymodule-alias", required_argument, 0, LONG_ARGS_POST_PYMODULE_ALIAS},
	{"import", required_argument, 0, LONG_ARGS_PYIMPORT},
	{"pyimport", required_argument, 0, LONG_ARGS_PYIMPORT},
	{"py-import", required_argument, 0, LONG_ARGS_PYIMPORT},
	{"python-import", required_argument, 0, LONG_ARGS_PYIMPORT},
	{"spooler-import", required_argument, 0, LONG_ARGS_SPOOLER_PYIMPORT},
	{"spooler-pyimport", required_argument, 0, LONG_ARGS_SPOOLER_PYIMPORT},
	{"spooler-py-import", required_argument, 0, LONG_ARGS_SPOOLER_PYIMPORT},
	{"spooler-python-import", required_argument, 0, LONG_ARGS_SPOOLER_PYIMPORT},
	{"pp", required_argument, 0, LONG_ARGS_PYTHONPATH},
	{"pyargv", required_argument, 0, LONG_ARGS_PYARGV},
	{"optimize", required_argument, 0, 'O'},
	{"paste", required_argument, 0, LONG_ARGS_PASTE},
	{"web3", required_argument, 0, LONG_ARGS_WEB3},
	{"pump", required_argument, 0, LONG_ARGS_PUMP},
	{"wsgi-lite", required_argument, 0, LONG_ARGS_WSGI_LITE},
#ifdef UWSGI_INI
	{"ini-paste", required_argument, 0, LONG_ARGS_INI_PASTE},
#endif
	{"catch-exceptions", no_argument, &up.catch_exceptions, 1},
	{"ignore-script-name", no_argument, &up.ignore_script_name, 1},
	{"pep3333-input", no_argument, &up.pep3333_input, 1},
	{"reload-os-env", no_argument, &up.reload_os_env, 1},
#ifndef UWSGI_PYPY
	{"no-site", no_argument, &Py_NoSiteFlag, 1},
#endif
	{"pyshell", no_argument, 0, LONG_ARGS_PYSHELL},
	{"python", required_argument, 0, LONG_ARGS_PYTHON_RUN},
	{"py", required_argument, 0, LONG_ARGS_PYTHON_RUN},

	{0, 0, 0, 0},
};

struct uwsgi_help_item uwsgi_python_help[] = {

{"module <module>" ,"name of python config module"},
{"optimize <n>", "set python optimization level to <n>"},
{"home <path>", "set python home/virtualenv"},
{"pyhome <path>", "set python home/virtualenv"},
{"virtualenv <path>", "set python home/virtualenv"},
{"venv <path>", "set python home/virtualenv"},
{"callable <callable>", "set the callable (default 'application')"},
{"paste <config:/egg:>", "load applications using paste.deploy.loadapp()"},
{"pythonpath <dir>", "add <dir> to PYTHONPATH"},
{"python-path <dir>", "add <dir> to PYTHONPATH"},
{"pp <dir>", "add <dir> to PYTHONPATH"},
{"pyargv <args>", "assign args to python sys.argv"},
{"wsgi-file <file>", "load the <file> wsgi file"},
{"file <file>", "use python file instead of python module for configuration"},
{"eval <code>", "evaluate code for app configuration"},
{"ini-paste <inifile>", "path of ini config file that contains paste configuration"},
{"pyshell", "run a python interactive shell in the uwsgi environment (steals a worker)"},


 { 0, 0},
};

/* this routine will be called after each fork to reinitialize the various locks */
void uwsgi_python_pthread_prepare(void) {
	pthread_mutex_lock(&up.lock_pyloaders);
}

void uwsgi_python_pthread_parent(void) {
	pthread_mutex_unlock(&up.lock_pyloaders);
}

void uwsgi_python_pthread_child(void) {
	pthread_mutex_init(&up.lock_pyloaders, NULL);
}

// fake method
PyMethodDef null_methods[] = {
	{NULL, NULL},
};

PyMethodDef uwsgi_spit_method[] = { {"uwsgi_spit", py_uwsgi_spit, METH_VARARGS, ""} };
PyMethodDef uwsgi_write_method[] = { {"uwsgi_write", py_uwsgi_write, METH_VARARGS, ""} };

int uwsgi_python_init() {

#ifndef UWSGI_PYPY
	char *pyversion = strchr(Py_GetVersion(), '\n');
        uwsgi_log_initial("Python version: %.*s %s\n", pyversion-Py_GetVersion(), Py_GetVersion(), Py_GetCompiler()+1);
#else
	uwsgi_log_initial("PyPy version: %s\n", PYPY_VERSION);
#endif

#ifndef UWSGI_PYPY
	if (up.home != NULL) {
#ifdef PYTHREE
		wchar_t *wpyhome;
		wpyhome = malloc((sizeof(wchar_t) * strlen(up.home)) + sizeof(wchar_t) );
		if (!wpyhome) {
			uwsgi_error("malloc()");
			exit(1);
		}
		mbstowcs(wpyhome, up.home, strlen(up.home));
		Py_SetPythonHome(wpyhome);
		// do not free this memory !!!
		//free(wpyhome);
#else
		Py_SetPythonHome(up.home);
#endif
		uwsgi_log("Set PythonHome to %s\n", up.home);
	}

#ifdef PYTHREE
	wchar_t pname[6];
	mbstowcs(pname, "uWSGI", 6);
	Py_SetProgramName(pname);
#else

	Py_SetProgramName("uWSGI");
#endif


	Py_OptimizeFlag = up.optimize;

	Py_Initialize();


#endif

	up.wsgi_spitout = PyCFunction_New(uwsgi_spit_method, NULL);
	up.wsgi_writeout = PyCFunction_New(uwsgi_write_method, NULL);

	up.main_thread = PyThreadState_Get();

        // by default set a fake GIL (little impact on performance)
        up.gil_get = gil_fake_get;
        up.gil_release = gil_fake_release;

        up.swap_ts = simple_swap_ts;
        up.reset_ts = simple_reset_ts;
	

	uwsgi_log_initial("Python main interpreter initialized at %p\n", up.main_thread);

	return 1;

}

void uwsgi_python_reset_random_seed() {

#ifndef UWSGI_PYPY
	PyObject *random_module, *random_dict, *random_seed;

        // reinitialize the random seed (thanks Jonas Borgström)
        random_module = PyImport_ImportModule("random");
        if (random_module) {
                random_dict = PyModule_GetDict(random_module);
                if (random_dict) {
                        random_seed = PyDict_GetItemString(random_dict, "seed");
                        if (random_seed) {
                                PyObject *random_args = PyTuple_New(1);
                                // pass no args
                                PyTuple_SetItem(random_args, 0, Py_None);
                                PyEval_CallObject(random_seed, random_args);
                                if (PyErr_Occurred()) {
                                        PyErr_Print();
                                }
                        }
                }
        }
#endif
}

void uwsgi_python_post_fork() {

#ifdef UWSGI_SPOOLER
	if (uwsgi.shared->spooler_pid > 0) {
		if (uwsgi.shared->spooler_pid == getpid()) {
			UWSGI_GET_GIL
		}
	}	
#endif

	uwsgi_python_reset_random_seed();

#ifndef UWSGI_PYPY
#ifdef UWSGI_EMBEDDED
	// call the post_fork_hook
	PyObject *uwsgi_dict = get_uwsgi_pydict("uwsgi");
	if (uwsgi_dict) {
		PyObject *pfh = PyDict_GetItemString(uwsgi_dict, "post_fork_hook");
		if (pfh) {
			python_call(pfh, PyTuple_New(0), 0, NULL);
		}
	}
	PyErr_Clear();
#endif
#endif

UWSGI_RELEASE_GIL

}

PyObject *uwsgi_pyimport_by_filename(char *name, char *filename) {

#ifndef UWSGI_PYPY
	FILE *pyfile;
	struct _node *py_file_node = NULL;
	PyObject *py_compiled_node, *py_file_module;
	int is_a_package = 0;
	struct stat pystat;
	char *real_filename = filename;


	if (!uwsgi_check_scheme(filename)) {

		pyfile = fopen(filename, "r");
		if (!pyfile) {
			uwsgi_log("failed to open python file %s\n", filename);
			return NULL;
		}

		if (fstat(fileno(pyfile), &pystat)) {
			uwsgi_error("fstat()");
			return NULL;
		}

		if (S_ISDIR(pystat.st_mode)) {
			is_a_package = 1;
			fclose(pyfile);
			real_filename = uwsgi_concat2(filename, "/__init__.py");
			pyfile = fopen(real_filename, "r");
			if (!pyfile) {
				uwsgi_error_open(real_filename);
				free(real_filename);
				fclose(pyfile);
				return NULL;
			}
		}

		py_file_node = PyParser_SimpleParseFile(pyfile, real_filename, Py_file_input);
		if (!py_file_node) {
			PyErr_Print();
			uwsgi_log("failed to parse file %s\n", real_filename);
			if (is_a_package)
				free(real_filename);
			fclose(pyfile);
			return NULL;
		}

		fclose(pyfile);
	}
	else {
		int pycontent_size = 0;
		char *pycontent = uwsgi_open_and_read(filename, &pycontent_size, 1, NULL);

		if (pycontent) {
			py_file_node = PyParser_SimpleParseString(pycontent, Py_file_input);
			if (!py_file_node) {
				PyErr_Print();
				uwsgi_log("failed to parse url %s\n", real_filename);
				return NULL;
			}
		}
	}

	py_compiled_node = (PyObject *) PyNode_Compile(py_file_node, real_filename);

	if (!py_compiled_node) {
		PyErr_Print();
		uwsgi_log("failed to compile python file %s\n", real_filename);
		return NULL;
	}

	py_file_module = PyImport_ExecCodeModule(name, py_compiled_node);
	if (!py_file_module) {
		PyErr_Print();
		return NULL;
	}

	Py_DECREF(py_compiled_node);

	if (is_a_package) {
		PyObject *py_file_module_dict = PyModule_GetDict(py_file_module);
		if (py_file_module_dict) {
			PyDict_SetItemString(py_file_module_dict, "__path__", Py_BuildValue("[O]", PyString_FromString(filename)));
		}
		free(real_filename);
	}

	return py_file_module;
#else
	return NULL;
#endif

}

void init_uwsgi_vars() {

	int i;
	PyObject *pysys, *pysys_dict, *pypath;

	PyObject *modules = PyImport_GetModuleDict();
	PyObject *tmp_module;

	/* add cwd to pythonpath */
	pysys = PyImport_ImportModule("sys");
	if (!pysys) {
		PyErr_Print();
		exit(1);
	}
	pysys_dict = PyModule_GetDict(pysys);

#ifdef PYTHREE
	// fix stdout and stderr
	PyObject *new_stdprint = PyFile_NewStdPrinter(2);
	PyDict_SetItemString(pysys_dict, "stdout", new_stdprint);
	PyDict_SetItemString(pysys_dict, "__stdout__", new_stdprint);
	PyDict_SetItemString(pysys_dict, "stderr", new_stdprint);
	PyDict_SetItemString(pysys_dict, "__stderr__", new_stdprint);
#endif
	pypath = PyDict_GetItemString(pysys_dict, "path");
	if (!pypath) {
		PyErr_Print();
		exit(1);
	}

	if (PyList_Insert(pypath, 0, UWSGI_PYFROMSTRING(".")) != 0) {
		PyErr_Print();
	}

	struct uwsgi_string_list *uppp = up.python_path;
	while(uppp) {
		if (PyList_Insert(pypath, 0, UWSGI_PYFROMSTRING(uppp->value)) != 0) {
			PyErr_Print();
		}
		else {
			uwsgi_log("added %s to pythonpath.\n", uppp->value);
		}

		uppp = uppp->next;
	}

	for (i = 0; i < up.pymodule_alias_cnt; i++) {
		// split key=value
		char *value = strchr(up.pymodule_alias[i], '=');
		if (!value) {
			uwsgi_log("invalid pymodule-alias syntax\n");
			continue;
		}
		value[0] = 0;
		if (!strchr(value + 1, '/')) {
			// this is a standard pymodule
			tmp_module = PyImport_ImportModule(value + 1);
			if (!tmp_module) {
				PyErr_Print();
				exit(1);
			}

			PyDict_SetItemString(modules, up.pymodule_alias[i], tmp_module);
		}
		else {
			// this is a filepath that need to be mapped
			tmp_module = uwsgi_pyimport_by_filename(up.pymodule_alias[i], value + 1);
			if (!tmp_module) {
				PyErr_Print();
				exit(1);
			}
		}
		uwsgi_log("mapped virtual pymodule \"%s\" to real pymodule \"%s\"\n", up.pymodule_alias[i], value + 1);
		// reset original value
		value[0] = '=';
	}

}



#ifdef PYTHREE
static PyModuleDef uwsgi_module3 = {
	PyModuleDef_HEAD_INIT,
	"uwsgi",
	NULL,
	-1,
	null_methods,
};
PyObject *init_uwsgi3(void) {
	return PyModule_Create(&uwsgi_module3);
}
#endif


#ifdef UWSGI_EMBEDDED
void init_uwsgi_embedded_module() {
	PyObject *new_uwsgi_module, *zero;
	int i;

	PyType_Ready(&uwsgi_InputType);

	/* initialize for stats */
	up.workers_tuple = PyTuple_New(uwsgi.numproc);
	for (i = 0; i < uwsgi.numproc; i++) {
		zero = PyDict_New();
		Py_INCREF(zero);
		PyTuple_SetItem(up.workers_tuple, i, zero);
	}



#ifdef PYTHREE
	PyImport_AppendInittab("uwsgi", init_uwsgi3);
	new_uwsgi_module = PyImport_AddModule("uwsgi");
#else
	new_uwsgi_module = Py_InitModule("uwsgi", null_methods);
#endif
	if (new_uwsgi_module == NULL) {
		uwsgi_log("could not initialize the uwsgi python module\n");
		exit(1);
	}

	Py_INCREF((PyObject *) &uwsgi_InputType);

	up.embedded_dict = PyModule_GetDict(new_uwsgi_module);
	if (!up.embedded_dict) {
		uwsgi_log("could not get uwsgi module __dict__\n");
		exit(1);
	}

	// just for safety
	Py_INCREF(up.embedded_dict);

	if (PyDict_SetItemString(up.embedded_dict, "version", PyString_FromString(UWSGI_VERSION))) {
		PyErr_Print();
		exit(1);
	}

	PyObject *uwsgi_py_version_info = PyTuple_New(5);

	PyTuple_SetItem(uwsgi_py_version_info, 0, PyInt_FromLong(UWSGI_VERSION_BASE));
	PyTuple_SetItem(uwsgi_py_version_info, 1, PyInt_FromLong(UWSGI_VERSION_MAJOR));
	PyTuple_SetItem(uwsgi_py_version_info, 2, PyInt_FromLong(UWSGI_VERSION_MINOR));
	PyTuple_SetItem(uwsgi_py_version_info, 3, PyInt_FromLong(UWSGI_VERSION_REVISION));
	PyTuple_SetItem(uwsgi_py_version_info, 4, PyString_FromString(UWSGI_VERSION_CUSTOM));

	if (PyDict_SetItemString(up.embedded_dict, "version_info", uwsgi_py_version_info)) {
		PyErr_Print();
		exit(1);
	}


	if (PyDict_SetItemString(up.embedded_dict, "hostname", PyString_FromStringAndSize(uwsgi.hostname, uwsgi.hostname_len))) {
		PyErr_Print();
		exit(1);
	}

	if (uwsgi.mode) {
		if (PyDict_SetItemString(up.embedded_dict, "mode", PyString_FromString(uwsgi.mode))) {
			PyErr_Print();
			exit(1);
		}
	}

	if (uwsgi.pidfile) {
		if (PyDict_SetItemString(up.embedded_dict, "pidfile", PyString_FromString(uwsgi.pidfile))) {
			PyErr_Print();
			exit(1);
		}
	}


	if (PyDict_SetItemString(up.embedded_dict, "SPOOL_RETRY", PyInt_FromLong(-1))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "SPOOL_OK", PyInt_FromLong(-2))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "SPOOL_IGNORE", PyInt_FromLong(0))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "numproc", PyInt_FromLong(uwsgi.numproc))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "has_threads", PyInt_FromLong(uwsgi.has_threads))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "cores", PyInt_FromLong(uwsgi.cores))) {
		PyErr_Print();
		exit(1);
	}

	if (uwsgi.loop) {
		if (PyDict_SetItemString(up.embedded_dict, "loop", PyString_FromString(uwsgi.loop))) {
			PyErr_Print();
			exit(1);
		}
	}

	PyObject *py_opt_dict = PyDict_New();
	for (i = 0; i < uwsgi.exported_opts_cnt; i++) {
		if (PyDict_Contains(py_opt_dict, PyString_FromString(uwsgi.exported_opts[i]->key))) {
			PyObject *py_opt_item = PyDict_GetItemString(py_opt_dict, uwsgi.exported_opts[i]->key);
			if (PyList_Check(py_opt_item)) {
				if (uwsgi.exported_opts[i]->value == NULL) {
					PyList_Append(py_opt_item, Py_True);
				}
				else {
					PyList_Append(py_opt_item, PyString_FromString(uwsgi.exported_opts[i]->value));
				}
			}
			else {
				PyObject *py_opt_list = PyList_New(0);
				PyList_Append(py_opt_list, py_opt_item);
				if (uwsgi.exported_opts[i]->value == NULL) {
					PyList_Append(py_opt_list, Py_True);
				}
				else {
					PyList_Append(py_opt_list, PyString_FromString(uwsgi.exported_opts[i]->value));
				}

				PyDict_SetItemString(py_opt_dict, uwsgi.exported_opts[i]->key, py_opt_list);
			}
		}
		else {
			if (uwsgi.exported_opts[i]->value == NULL) {
				PyDict_SetItemString(py_opt_dict, uwsgi.exported_opts[i]->key, Py_True);
			}
			else {
				PyDict_SetItemString(py_opt_dict, uwsgi.exported_opts[i]->key, PyString_FromString(uwsgi.exported_opts[i]->value));
			}
		}
	}

	if (PyDict_SetItemString(up.embedded_dict, "opt", py_opt_dict)) {
		PyErr_Print();
		exit(1);
	}

	PyObject *py_magic_table = PyDict_New();
	uint8_t mtk;
	for (i = 0; i <= 0xff; i++) {
		// a bit of magic :P
		mtk = i;
                if (uwsgi.magic_table[i]) {
			if (uwsgi.magic_table[i][0] != 0) {
				PyDict_SetItem(py_magic_table, PyString_FromStringAndSize((char *) &mtk, 1), PyString_FromString(uwsgi.magic_table[i]));
			}
		}
        }

	if (PyDict_SetItemString(up.embedded_dict, "magic_table", py_magic_table)) {
		PyErr_Print();
		exit(1);
	}

#ifdef UNBIT
	if (PyDict_SetItemString(up.embedded_dict, "unbit", Py_True)) {
#else
	if (PyDict_SetItemString(up.embedded_dict, "unbit", Py_None)) {
#endif
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "buffer_size", PyInt_FromLong(uwsgi.buffer_size))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "started_on", PyInt_FromLong(uwsgi.start_tv.tv_sec))) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "start_response", up.wsgi_spitout)) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "fastfuncs", PyList_New(256))) {
		PyErr_Print();
		exit(1);
	}


	if (PyDict_SetItemString(up.embedded_dict, "applications", Py_None)) {
		PyErr_Print();
		exit(1);
	}

	if (uwsgi.is_a_reload) {
		if (PyDict_SetItemString(up.embedded_dict, "is_a_reload", Py_True)) {
			PyErr_Print();
			exit(1);
		}
	}
	else {
		if (PyDict_SetItemString(up.embedded_dict, "is_a_reload", Py_False)) {
			PyErr_Print();
			exit(1);
		}
	}

	up.embedded_args = PyTuple_New(2);
	if (!up.embedded_args) {
		PyErr_Print();
		exit(1);
	}

	if (PyDict_SetItemString(up.embedded_dict, "message_manager_marshal", Py_None)) {
		PyErr_Print();
		exit(1);
	}

	up.fastfuncslist = PyDict_GetItemString(up.embedded_dict, "fastfuncs");
	if (!up.fastfuncslist) {
		PyErr_Print();
		exit(1);
	}

	init_uwsgi_module_advanced(new_uwsgi_module);

#ifdef UWSGI_SPOOLER
	if (uwsgi.spool_dir != NULL) {
		init_uwsgi_module_spooler(new_uwsgi_module);
	}
#endif


	if (uwsgi.sharedareasize > 0 && uwsgi.sharedarea) {
		init_uwsgi_module_sharedarea(new_uwsgi_module);
	}

	if (uwsgi.cache_max_items > 0) {
		init_uwsgi_module_cache(new_uwsgi_module);
	}

	if (uwsgi.queue_size > 0) {
		init_uwsgi_module_queue(new_uwsgi_module);
	}

#ifdef UWSGI_SNMP
	if (uwsgi.snmp) {
		init_uwsgi_module_snmp(new_uwsgi_module);
	}
#endif

	if (up.extension) {
		up.extension();
	}
}
#endif



int uwsgi_python_magic(char *mountpoint, char *lazy) {

	char *qc = strchr(lazy, ':');
	if (qc) {
		qc[0] = 0;
		up.callable = qc + 1;
	}

	if (!strcmp(lazy + strlen(lazy) - 3, ".py")) {
		up.file_config = lazy;
		return 1;
	}
	else if (!strcmp(lazy + strlen(lazy) - 5, ".wsgi")) {
		up.file_config = lazy;
		return 1;
	}
	else if (qc && strchr(lazy, '.')) {
		up.wsgi_config = lazy;
		return 1;
	}

	// reset lazy
	if (qc) {
		qc[0] = ':';
	}
	return 0;

}

int uwsgi_python_manage_options(int i, char *optarg) {

	glob_t g;
	int j;

	switch (i) {
	case 'w':
		up.wsgi_config = optarg;
		return 1;
	case LONG_ARGS_WSGI_FILE:
	case LONG_ARGS_FILE_CONFIG:
		up.file_config = optarg;
		return 1;
	case LONG_ARGS_EVAL_CONFIG:
		up.eval = optarg;
		return 1;
	case LONG_ARGS_PYMODULE_ALIAS:
		if (up.pymodule_alias_cnt < MAX_PYMODULE_ALIAS) {
			up.pymodule_alias[up.pymodule_alias_cnt] = optarg;
			up.pymodule_alias_cnt++;
		}
		else {
			uwsgi_log("you can specify at most %d --pymodule-alias options\n", MAX_PYMODULE_ALIAS);
		}
		return 1;
	case LONG_ARGS_PYSHELL:
		uwsgi.honour_stdin = 1;
		up.pyshell = 1;
		return 1;
	case LONG_ARGS_PYIMPORT:
		uwsgi_string_new_list(&up.import_list, optarg);
		return 1;
	case LONG_ARGS_SPOOLER_PYIMPORT:
		uwsgi_string_new_list(&up.spooler_import_list, optarg);
		return 1;
	case LONG_ARGS_POST_PYMODULE_ALIAS:
		uwsgi_string_new_list(&up.post_pymodule_alias, optarg);
		return 1;
	case LONG_ARGS_PYTHONPATH:
		if (glob(optarg, GLOB_MARK, NULL, &g)) {
			uwsgi_string_new_list(&up.python_path, optarg);
		}
		else {
			for (j = 0; j < (int) g.gl_pathc; j++) {
				uwsgi_string_new_list(&up.python_path, g.gl_pathv[j]);
			}
		}
		return 1;
	case LONG_ARGS_PYARGV:
		up.argv = optarg;
		return 1;
	case 'J':
		up.test_module = optarg;
		return 1;
	case 'H':
		up.home = optarg;
		return 1;
	case 'O':
		up.optimize = atoi(optarg);
		return 1;
	case LONG_ARGS_CALLABLE:
		up.callable = optarg;
		return 1;


#ifdef UWSGI_INI
	case LONG_ARGS_INI_PASTE:
		uwsgi_string_new_list(&uwsgi.ini,optarg);
		if (optarg[0] != '/') {
			up.paste = uwsgi_concat4("config:", uwsgi.cwd, "/", optarg);
		}
		else {
			up.paste = uwsgi_concat2("config:", optarg);
		}
		return 1;
#endif
	case LONG_ARGS_WEB3:
		up.web3 = optarg;
		return 1;
	case LONG_ARGS_PUMP:
		up.pump = optarg;
		return 1;
	case LONG_ARGS_WSGI_LITE:
		up.wsgi_lite = optarg;
		return 1;
	case LONG_ARGS_PASTE:
		up.paste = optarg;
		return 1;



	}

	return 0;
}

int uwsgi_python_mount_app(char *mountpoint, char *app, int regexp) {

	int id;
	uwsgi.wsgi_req->appid = mountpoint;
	uwsgi.wsgi_req->appid_len = strlen(mountpoint);
	if (uwsgi.single_interpreter) {
		id = init_uwsgi_app(LOADER_MOUNT, app, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WSGI);
	}
	id = init_uwsgi_app(LOADER_MOUNT, app, uwsgi.wsgi_req, NULL, PYTHON_APP_TYPE_WSGI);

#ifdef UWSGI_PCRE
	int i;
	if (regexp && id != -1) {
		struct uwsgi_app *ua = &uwsgi_apps[id];
		uwsgi_regexp_build(mountpoint, &ua->pattern, &ua->pattern_extra);
		if (uwsgi.mywid == 0) {
			for(i=1;i<=uwsgi.numproc;i++) {
				uwsgi.workers[i].apps[id].pattern = ua->pattern;
				uwsgi.workers[i].apps[id].pattern_extra = ua->pattern_extra;
			}
		}
	}
#endif

	return id;

}

char *uwsgi_pythonize(char *orig) {

	char *name = uwsgi_concat2(orig, "");
	size_t i;
	size_t len = 0;

	if (!strncmp(name, "sym://", 6)) {
		name+=6;
	}
	else if (!strncmp(name, "http://", 7)) {
		name+=7;
	}
	else if (!strncmp(name, "data://", 7)) {
		name+=7;
	}

	len = strlen(name);
	for(i=0;i<len;i++) {
		if (name[i] == '.') {
			name[i] = '_';
		}
		else if (name[i] == '/') {
			name[i] = '_';
		}
	}


	if ((name[len-3] == '.' || name[len-3] == '_') && name[len-2] == 'p' && name[len-1] == 'y') {
		name[len-3] = 0;
	}

	return name;

}

void uwsgi_python_spooler_init(void) {

	struct uwsgi_string_list *upli = up.spooler_import_list;

	UWSGI_GET_GIL

        while(upli) {
                if (strchr(upli->value, '/') || uwsgi_endswith(upli->value, ".py")) {
                        uwsgi_pyimport_by_filename(uwsgi_pythonize(upli->value), upli->value);
                }
                else {
                        if (PyImport_ImportModule(upli->value) == NULL) {
                                PyErr_Print();
                        }
                }
                upli = upli->next;
        }

	UWSGI_RELEASE_GIL
	

}

void uwsgi_python_init_apps() {

	struct http_status_codes *http_sc;


	if (uwsgi.async > 1) {
		up.current_recursion_depth = uwsgi_malloc(sizeof(int)*uwsgi.async);
#ifndef UWSGI_PYPY
        	up.current_frame = uwsgi_malloc(sizeof(struct _frame)*uwsgi.async);
#endif
	}

	init_pyargv();

#ifndef UWSGI_PYPY
#ifdef UWSGI_EMBEDDED
        init_uwsgi_embedded_module();
#endif
#endif


#ifdef __linux__
#ifndef UWSGI_PYPY
#ifdef UWSGI_EMBEDDED
	uwsgi_init_symbol_import();
#endif
#endif
#endif

        if (up.test_module != NULL) {
                if (PyImport_ImportModule(up.test_module)) {
                        exit(0);
                }
                exit(1);
        }

        init_uwsgi_vars();

        // setup app loaders
#ifdef UWSGI_MINTERPRETERS
        up.loaders[LOADER_DYN] = uwsgi_dyn_loader;
#endif
        up.loaders[LOADER_UWSGI] = uwsgi_uwsgi_loader;
        up.loaders[LOADER_FILE] = uwsgi_file_loader;
        up.loaders[LOADER_PASTE] = uwsgi_paste_loader;
        up.loaders[LOADER_EVAL] = uwsgi_eval_loader;
        up.loaders[LOADER_MOUNT] = uwsgi_mount_loader;
        up.loaders[LOADER_CALLABLE] = uwsgi_callable_loader;
        up.loaders[LOADER_STRING_CALLABLE] = uwsgi_string_callable_loader;


	struct uwsgi_string_list *upli = up.import_list;
	while(upli) {
		if (strchr(upli->value, '/') || uwsgi_endswith(upli->value, ".py")) {
			uwsgi_pyimport_by_filename(uwsgi_pythonize(upli->value), upli->value);
		}
		else {
			if (PyImport_ImportModule(upli->value) == NULL) {
				PyErr_Print();
			}
		}
		upli = upli->next;
	}

	struct uwsgi_string_list *uppa = up.post_pymodule_alias;
	PyObject *modules = PyImport_GetModuleDict();
	PyObject *tmp_module;
	while(uppa) {
                // split key=value
                char *value = strchr(uppa->value, '=');
                if (!value) {
                        uwsgi_log("invalid pymodule-alias syntax\n");
                        continue;
                }
                value[0] = 0;
                if (!strchr(value + 1, '/')) {
                        // this is a standard pymodule
                        tmp_module = PyImport_ImportModule(value + 1);
                        if (!tmp_module) {
                                PyErr_Print();
                                exit(1);
                        }

                        PyDict_SetItemString(modules, uppa->value, tmp_module);
                }
                else {
                        // this is a filepath that need to be mapped
                        tmp_module = uwsgi_pyimport_by_filename(uppa->value, value + 1);
                        if (!tmp_module) {
                                PyErr_Print();
                                exit(1);
                        }
                }
                uwsgi_log("mapped virtual pymodule \"%s\" to real pymodule \"%s\"\n", uppa->value, value + 1);
                // reset original value
                value[0] = '=';

		uppa = uppa->next;
        }


	if (up.wsgi_config != NULL) {
		init_uwsgi_app(LOADER_UWSGI, up.wsgi_config, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WSGI);
	}

	if (up.file_config != NULL) {
		init_uwsgi_app(LOADER_FILE, up.file_config, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WSGI);
	}
	if (up.paste != NULL) {
		init_uwsgi_app(LOADER_PASTE, up.paste, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WSGI);
	}
	if (up.eval != NULL) {
		init_uwsgi_app(LOADER_EVAL, up.eval, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WSGI);
	}
	if (up.web3 != NULL) {
		init_uwsgi_app(LOADER_UWSGI, up.web3, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WEB3);
	}
	if (up.pump != NULL) {
		init_uwsgi_app(LOADER_UWSGI, up.pump, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_PUMP);
		// filling http status codes
        	for (http_sc = hsc; http_sc->message != NULL; http_sc++) {
                	http_sc->message_size = (int) strlen(http_sc->message);
        	}
	}
	if (up.wsgi_lite != NULL) {
		init_uwsgi_app(LOADER_UWSGI, up.wsgi_lite, uwsgi.wsgi_req, up.main_thread, PYTHON_APP_TYPE_WSGI_LITE);
	}

	if (uwsgi.profiler) {
#ifndef UWSGI_PYPY
		if (!strcmp(uwsgi.profiler, "pycall")) {
			PyEval_SetProfile(uwsgi_python_profiler_call, NULL);
		}
#endif
	}

}

void uwsgi_python_master_fixup(int step) {

	static int master_fixed = 0;
	static int worker_fixed = 0;

	if (!uwsgi.master_process) return;

	if (uwsgi.has_threads) {
		if (step == 0) {
			if (!master_fixed) {
				UWSGI_RELEASE_GIL;
				master_fixed = 1;
			}
		}	
		else {
			if (!worker_fixed) {
				UWSGI_GET_GIL;
				worker_fixed = 1;
			}
		}
	}
}

void uwsgi_python_enable_threads() {

	PyEval_InitThreads();
	if (pthread_key_create(&up.upt_save_key, NULL)) {
		uwsgi_error("pthread_key_create()");
		exit(1);
	}
	if (pthread_key_create(&up.upt_gil_key, NULL)) {
		uwsgi_error("pthread_key_create()");
		exit(1);
	}
	pthread_setspecific(up.upt_save_key, (void *) PyThreadState_Get());
	pthread_setspecific(up.upt_gil_key, (void *) PyThreadState_Get());
	pthread_mutex_init(&up.lock_pyloaders, NULL);
	pthread_atfork(uwsgi_python_pthread_prepare, uwsgi_python_pthread_parent, uwsgi_python_pthread_child);

	up.gil_get = gil_real_get;
	up.gil_release = gil_real_release;

	if (uwsgi.threads > 1) {
		up.swap_ts = threaded_swap_ts;
		up.reset_ts = threaded_reset_ts;
	}

	uwsgi_log("threads support enabled\n");
	

}

void uwsgi_python_init_thread(int core_id) {

	// set a new ThreadState for each thread
	PyThreadState *pts;
	pts = PyThreadState_New(up.main_thread->interp);
	pthread_setspecific(up.upt_save_key, (void *) pts);
	pthread_setspecific(up.upt_gil_key, (void *) pts);
#ifdef UWSGI_DEBUG
	uwsgi_log("python ThreadState %d = %p\n", core_id, pts);
#endif
	UWSGI_GET_GIL;
	// call threading.currentThread (taken from mod_wsgi, but removes DECREFs as thread in uWSGI are fixed)
	PyObject *threading_module = PyImport_ImportModule("threading");
        if (threading_module) {
        	PyObject *threading_module_dict = PyModule_GetDict(threading_module);
                if (threading_module_dict) {
#ifdef PYTHREE
			PyObject *threading_current = PyDict_GetItemString(threading_module_dict, "current_thread");
#else
			PyObject *threading_current = PyDict_GetItemString(threading_module_dict, "currentThread");
#endif
                        if (threading_current) {
                                PyObject *current_thread = PyEval_CallObject(threading_current, (PyObject *)NULL);
                                if (!current_thread) {
					// ignore the error
                                        PyErr_Clear();
                                }
				else {
					PyObject_SetAttrString(current_thread, "name", PyString_FromFormat("uWSGIWorker%dCore%d", uwsgi.mywid, core_id));
					Py_INCREF(current_thread);
				}
                        }
                }
        }
	UWSGI_RELEASE_GIL;
	

}

int uwsgi_python_xml(char *node, char *content) {

	PyThreadState *interpreter = NULL;

	if (uwsgi.single_interpreter) {
		interpreter = up.main_thread;
	}

	if (!strcmp("script", node)) {
		return init_uwsgi_app(LOADER_UWSGI, content, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
	}
	else if (!strcmp("file", node)) {
		return init_uwsgi_app(LOADER_FILE, content, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
	}
	else if (!strcmp("eval", node)) {
		return init_uwsgi_app(LOADER_EVAL, content, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
	}
	else if (!strcmp("wsgi", node)) {
		return init_uwsgi_app(LOADER_EVAL, content, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
	}
	else if (!strcmp("module", node)) {
		uwsgi.wsgi_req->module = content;
		uwsgi.wsgi_req->module_len = strlen(content);
		uwsgi.wsgi_req->callable = strchr(uwsgi.wsgi_req->module, ':');
		if (uwsgi.wsgi_req->callable) {
			uwsgi.wsgi_req->callable[0] = 0;
			uwsgi.wsgi_req->callable++;
			uwsgi.wsgi_req->callable_len = strlen(uwsgi.wsgi_req->callable);
			uwsgi.wsgi_req->module_len = strlen(uwsgi.wsgi_req->module);
			return init_uwsgi_app(LOADER_DYN, uwsgi.wsgi_req, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
		}
		else {
			return init_uwsgi_app(LOADER_UWSGI, content, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
		}
		return 1;
	}
	else if (!strcmp("pyhome", node)) {
		uwsgi.wsgi_req->pyhome = content;
		uwsgi.wsgi_req->pyhome_len = strlen(content);
		return 1;
	}
	else if (!strcmp("callable", node)) {
		uwsgi.wsgi_req->callable = content;
		uwsgi.wsgi_req->callable_len = strlen(content);
		return init_uwsgi_app(LOADER_DYN, uwsgi.wsgi_req, uwsgi.wsgi_req, interpreter, PYTHON_APP_TYPE_WSGI);
	}

	return 0;
}

#ifndef UWSGI_PYPY
void uwsgi_python_suspend(struct wsgi_request *wsgi_req) {

	PyThreadState *tstate = PyThreadState_GET();

	if (wsgi_req) {
		up.current_recursion_depth[wsgi_req->async_id] = tstate->recursion_depth;
		up.current_frame[wsgi_req->async_id] = tstate->frame;
	}
	else {
		up.current_main_recursion_depth = tstate->recursion_depth;
		up.current_main_frame = tstate->frame;
	}

}
#endif

char *uwsgi_python_code_string(char *id, char *code, char *function, char *key, uint16_t keylen) {

	PyObject *cs_module = NULL;
	PyObject *cs_dict = NULL;

	UWSGI_GET_GIL;

	cs_module = PyImport_ImportModule(id);
	if (!cs_module) {
		PyErr_Clear();
		cs_module = uwsgi_pyimport_by_filename(id, code);
	}

	if (!cs_module) {
		UWSGI_RELEASE_GIL;
		return NULL;
	}

	cs_dict = PyModule_GetDict(cs_module);
	if (!cs_dict) {
		PyErr_Print();
		UWSGI_RELEASE_GIL;
		return NULL;
	}
	
	PyObject *func = PyDict_GetItemString(cs_dict, function);
	if (!func) {
		uwsgi_log("function %s not available in %s\n", function, code);
		PyErr_Print();
		UWSGI_RELEASE_GIL;
		return NULL;
	}

	PyObject *args = PyTuple_New(1);

	PyTuple_SetItem(args, 0, PyString_FromStringAndSize(key, keylen));

	PyObject *ret = python_call(func, args, 0, NULL);
	Py_DECREF(args);
	if (ret && PyString_Check(ret)) {
		char *val = PyString_AsString(ret);
		UWSGI_RELEASE_GIL;
		return val;
	}

	UWSGI_RELEASE_GIL;
	return NULL;
	
}

int uwsgi_python_signal_handler(uint8_t sig, void *handler) {

	UWSGI_GET_GIL;

	PyObject *args = PyTuple_New(1);
	PyObject *ret;

	if (!args)
		goto clear;

	if (!handler) goto clear;


	PyTuple_SetItem(args, 0, PyInt_FromLong(sig));

	ret = python_call(handler, args, 0, NULL);
	Py_DECREF(args);
	if (ret) {
		Py_DECREF(ret);
		UWSGI_RELEASE_GIL;
		return 0;
	}

clear:
	UWSGI_RELEASE_GIL;
	return -1;
}

uint16_t uwsgi_python_rpc(void *func, uint8_t argc, char **argv, char *buffer) {

	UWSGI_GET_GIL;

	uint8_t i;
	PyObject *pyargs = PyTuple_New(argc);
	PyObject *ret;
	char *rv;
	size_t rl;

	if (!pyargs)
		return 0;

	for (i = 0; i < argc; i++) {
		PyTuple_SetItem(pyargs, i, PyString_FromString(argv[i]));
	}

	ret = python_call((PyObject *) func, pyargs, 0, NULL);

	if (ret) {
		if (PyString_Check(ret)) {
			rv = PyString_AsString(ret);
			rl = PyString_Size(ret);
			if (rl <= 0xffff) {
				memcpy(buffer, rv, rl);
				Py_DECREF(ret);
				UWSGI_RELEASE_GIL;
				return rl;
			}
		}
	}

	if (PyErr_Occurred())
		PyErr_Print();

	UWSGI_RELEASE_GIL;

	return 0;

}

void uwsgi_python_add_item(char *key, uint16_t keylen, char *val, uint16_t vallen, void *data) {

	PyObject *pydict = (PyObject *) data;

	PyDict_SetItem(pydict, PyString_FromStringAndSize(key, keylen), PyString_FromStringAndSize(val, vallen));
}

int uwsgi_python_spooler(char *filename, char *buf, uint16_t len, char *body, size_t body_len) {

	static int random_seed_reset = 0;

	UWSGI_GET_GIL;

	PyObject *spool_dict = PyDict_New();
	PyObject *spool_func, *pyargs, *ret;

	if (!random_seed_reset) {
		uwsgi_python_reset_random_seed();
		random_seed_reset = 1;	
	}

	if (!up.embedded_dict) {
		// ignore
		UWSGI_RELEASE_GIL;
		return 0;
	}

	spool_func = PyDict_GetItemString(up.embedded_dict, "spooler");
	if (!spool_func) {
		// ignore
		UWSGI_RELEASE_GIL;
		return 0;
	}

	if (uwsgi_hooked_parse(buf, len, uwsgi_python_add_item, spool_dict)) {
		// malformed packet, destroy it
		UWSGI_RELEASE_GIL;
		return -2;
	}

	pyargs = PyTuple_New(1);

	PyDict_SetItemString(spool_dict, "spooler_task_name", PyString_FromString(filename));

	if (body && body_len > 0) {
		PyDict_SetItemString(spool_dict, "body", PyString_FromStringAndSize(body, body_len));
	}
	PyTuple_SetItem(pyargs, 0, spool_dict);

	ret = python_call(spool_func, pyargs, 0, NULL);

	if (ret) {
		if (!PyInt_Check(ret)) {
			// error, retry
			UWSGI_RELEASE_GIL;
			return -1;
		}	

		int retval = (int) PyInt_AsLong(ret);
		UWSGI_RELEASE_GIL;
		return retval;
		
	}
	
	if (PyErr_Occurred())
		PyErr_Print();

	// error, retry
	UWSGI_RELEASE_GIL;
	return -1;
}

#ifndef UWSGI_PYPY
void uwsgi_python_resume(struct wsgi_request *wsgi_req) {

	PyThreadState *tstate = PyThreadState_GET();

	if (wsgi_req) {
		tstate->recursion_depth = up.current_recursion_depth[wsgi_req->async_id];
		tstate->frame = up.current_frame[wsgi_req->async_id];
	}
	else {
		tstate->recursion_depth = up.current_main_recursion_depth;
		tstate->frame = up.current_main_frame;
	}

}
#endif

void uwsgi_python_fixup() {
	// set hacky modifier 30
	uwsgi.p[30] = uwsgi_malloc( sizeof(struct uwsgi_plugin) );
	memcpy(uwsgi.p[30], uwsgi.p[0], sizeof(struct uwsgi_plugin) );
	uwsgi.p[30]->init_thread = NULL;
}

void uwsgi_python_hijack(void) {
	// the pyshell will be execute only in the first worker

#ifndef UWSGI_PYPY
	if (up.pyshell && uwsgi.mywid == 1) {
		// re-map stdin to stdout and stderr if we are logging to a file
		if (uwsgi.logfile) {
			if (dup2(0, 1) < 0) {
				uwsgi_error("dup2()");
			}
			if (dup2(0, 2) < 0) {
				uwsgi_error("dup2()");
			}
		}
		UWSGI_GET_GIL;
		PyImport_ImportModule("readline");
		PyRun_InteractiveLoop(stdin, "uwsgi");
		exit(0);
	}
#endif
}

int uwsgi_python_mule(char *opt) {

	if (uwsgi_endswith(opt, ".py")) {
		UWSGI_GET_GIL;
		if (uwsgi_pyimport_by_filename("__main__", opt) == NULL) {
			return 0;
		}
		UWSGI_RELEASE_GIL;
		return 1;
	}
	
	return 0;
	
}

int uwsgi_python_mule_msg(char *message, size_t len) {

	UWSGI_GET_GIL;

	PyObject *mule_msg_hook = PyDict_GetItemString(up.embedded_dict, "mule_msg_hook");
        if (!mule_msg_hook) {
                // ignore
                UWSGI_RELEASE_GIL;
                return 0;
        }

	PyObject *pyargs = PyTuple_New(1);
        PyTuple_SetItem(pyargs, 0, PyString_FromStringAndSize(message, len));

        PyObject *ret = python_call(mule_msg_hook, pyargs, 0, NULL);
	Py_DECREF(pyargs);
	if (ret) {
		Py_DECREF(ret);
	}

	if (PyErr_Occurred())
                PyErr_Print();

	UWSGI_RELEASE_GIL;
	return 1;
}

struct uwsgi_plugin python_plugin = {

	.name = "python",
	.alias = "python",
	.modifier1 = 0,
	.init = uwsgi_python_init,
	.post_fork = uwsgi_python_post_fork,
	.options = uwsgi_python_options,
	.manage_opt = uwsgi_python_manage_options,
	.short_options = "w:O:H:J:",
	.request = uwsgi_request_wsgi,
	.after_request = uwsgi_after_request_wsgi,
	.init_apps = uwsgi_python_init_apps,

	.fixup = uwsgi_python_fixup,
	.master_fixup = uwsgi_python_master_fixup,

	.mount_app = uwsgi_python_mount_app,

	.enable_threads = uwsgi_python_enable_threads,
	.init_thread = uwsgi_python_init_thread,
	.manage_xml = uwsgi_python_xml,

	.magic = uwsgi_python_magic,

#ifndef UWSGI_PYPY
	.suspend = uwsgi_python_suspend,
	.resume = uwsgi_python_resume,
#endif

	.hijack_worker = uwsgi_python_hijack,
	.spooler_init = uwsgi_python_spooler_init,

	.signal_handler = uwsgi_python_signal_handler,
	.rpc = uwsgi_python_rpc,

	.mule = uwsgi_python_mule,
	.mule_msg = uwsgi_python_mule_msg,

	.spooler = uwsgi_python_spooler,

	.code_string = uwsgi_python_code_string,

	.help = uwsgi_python_help,

};
