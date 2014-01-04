IF (OSPRAY_MPI)
	if (OSPRAY_ICC)
		find_program(MPI_COMPILER 
			NAMES mpiicpc
			DOC "MPI compiler.")
	else()
		find_program(MPI_COMPILER 
			NAMES mpicxx
			DOC "MPI compiler.")
	endif()
	mark_as_advanced(MPI_COMPILER)

	exec_program(${MPI_COMPILER} 
		ARGS -show
		OUTPUT_VARIABLE MPI_COMPILE_CMDLINE
		RETURN_VALUE MPI_COMPILER_RETURN)
	#message("MPI_COMPILE_CMDLINE ${MPI_COMPILE_CMDLINE}")

	# Extract include paths from compile command line
	string(REGEX MATCHALL "-I([^\" ]+|\"[^\"]+\")" MPI_ALL_INCLUDE_PATHS "${MPI_COMPILE_CMDLINE}")
	set(MPI_INCLUDE_PATH_WORK)
	foreach(IPATH ${MPI_ALL_INCLUDE_PATHS})
		string(REGEX REPLACE "^-I" "" IPATH ${IPATH})
		string(REGEX REPLACE "//" "/" IPATH ${IPATH})
		list(APPEND MPI_INCLUDE_PATH_WORK ${IPATH})
	endforeach(IPATH)

	set(MPI_INCLUDE_PATH ${MPI_INCLUDE_PATH_WORK} )
	set(MPI_LIBRARIES 
		$ENV{I_MPI_ROOT}/intel64/lib/libmpi_mt.so
		)
	set(MPI_LIBRARIES_MIC
		$ENV{I_MPI_ROOT}/mic/lib/libmpi_mt.so
		)

	MACRO(CONFIGURE_MPI)
		INCLUDE_DIRECTORIES(${MPI_INCLUDE_PATH})
		SET(CMAKE_CXX_COMPILER ${MPI_COMPILER})
		SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mt_mpi")
		SET(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -mt_mpi")
	ENDMACRO()
ENDIF()