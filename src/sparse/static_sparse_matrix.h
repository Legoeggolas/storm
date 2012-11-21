#ifndef MRMC_SPARSE_STATIC_SPARSE_MATRIX_H_
#define MRMC_SPARSE_STATIC_SPARSE_MATRIX_H_

#include <exception>
#include <new>
#include "boost/integer/integer_mask.hpp"

#include <pantheios/pantheios.hpp>
#include <pantheios/inserters/integer.hpp>

#include "src/exceptions/invalid_state.h"
#include "src/exceptions/invalid_argument.h"
#include "src/exceptions/out_of_range.h"
#include "src/exceptions/file_IO_exception.h"

#include "src/misc/const_templates.h"

#include "Eigen/Sparse"

namespace mrmc {

namespace sparse {

/*!
 * A sparse matrix class with a constant number of non-zero entries on the non-diagonal fields
 * and a separate dense storage for the diagonal elements.
 * NOTE: Addressing *is* zero-based, so the valid range for getValue and addNextValue is 0..(rows - 1)
 * where rows is the first argument to the constructor.
 */
template<class T>
class StaticSparseMatrix {
public:
	/*!
	 * An enum representing the internal state of the Matrix.
	 * After creating the Matrix using the Constructor, the Object is in state UnInitialized. After calling initialize(), that state changes to Initialized and after all entries have been entered and finalize() has been called, to ReadReady.
	 * Should a critical error occur in any of the former functions, the state will change to Error.
	 * @see getState()
	 * @see isReadReady()
	 * @see isInitialized()
	 * @see hasError()
	 */
	enum MatrixStatus {
		Error = -1, UnInitialized = 0, Initialized = 1, ReadReady = 2,
	};

	//! Constructor
	/*!
	 * Constructs a sparse matrix object with the given number of rows.
	 * @param rows The number of rows of the matrix
	 */
	StaticSparseMatrix(uint_fast64_t rows)
			: internalStatus(MatrixStatus::UnInitialized),
			  currentSize(0), lastRow(0), valueStorage(nullptr),
			  diagonalStorage(nullptr), columnIndications(nullptr),
			  rowIndications(nullptr), rowCount(rows), nonZeroEntryCount(0) { }

	//! Copy Constructor
	/*!
	 * Copy Constructor. Performs a deep copy of the given sparse matrix.
	 * @param ssm A reference to the matrix to be copied.
	 */
	StaticSparseMatrix(const StaticSparseMatrix<T> &ssm)
			: internalStatus(ssm.internalStatus),
			  currentSize(ssm.currentSize), lastRow(ssm.lastRow),
			  rowCount(ssm.rowCount),
			  nonZeroEntryCount(ssm.nonZeroEntryCount) {
		pantheios::log_DEBUG("StaticSparseMatrix::CopyConstructor: Using copy constructor.");
		// Check whether copying the matrix is safe.
		if (!ssm.hasError()) {
			pantheios::log_ERROR("StaticSparseMatrix::CopyConstructor: Throwing invalid_argument: Can not copy from matrix in error state.");
			throw mrmc::exceptions::invalid_argument();
		} else {
			// Try to prepare the internal storage and throw an error in case
			// of a failure.
			if (!prepareInternalStorage()) {
				pantheios::log_ERROR("StaticSparseMatrix::CopyConstructor: Throwing bad_alloc: memory allocation failed.");
				throw std::bad_alloc();
			} else {
				// Now that all storages have been prepared, copy over all
				// elements. Start by copying the elements of type value and
				// copy them seperately in order to invoke copy their copy
				// constructor. This may not be necessary, but it is safer to
				// do so in any case.
				for (uint_fast64_t i = 0; i < nonZeroEntryCount; ++i) {
					// use T() to force use of the copy constructor for complex T types
					valueStorage[i] = T(ssm.valueStorage[i]);
				}
				for (uint_fast64_t i = 0; i <= rowCount; ++i) {
					// use T() to force use of the copy constructor for complex T types
					diagonalStorage[i] = T(ssm.diagonalStorage[i]);
				}

				// The elements that are not of the value type but rather the
				// index type may be copied with memcpy.
				memcpy(columnIndications, ssm.columnIndications, sizeof(columnIndications[0]) * nonZeroEntryCount);
				memcpy(rowIndications, ssm.rowIndications, sizeof(rowIndications[0]) * (rowCount + 1));
			}
		}
	}

	//! Destructor
	/*!
	 * Destructor. Performs deletion of the reserved storage arrays.
	 */
	~StaticSparseMatrix() {
		setState(MatrixStatus::UnInitialized);
		if (valueStorage != NULL) {
			//free(value_storage);
			delete[] valueStorage;
		}
		if (columnIndications != NULL) {
			//free(column_indications);
			delete[] columnIndications;
		}
		if (rowIndications != NULL) {
			//free(row_indications);
			delete[] rowIndications;
		}
		if (diagonalStorage != NULL) {
			//free(diagonal_storage);
			delete[] diagonalStorage;
		}
	}

	/*!
	 * Initializes the sparse matrix with the given number of non-zero entries
	 * and prepares it for use with addNextValue() and finalize().
	 * NOTE: Calling this method before any other member function is mandatory.
	 * This version is to be used together with addNextValue(). For
	 * initialization from an Eigen SparseMatrix, use initialize(Eigen::SparseMatrix<T> &).
	 * @param nonZeroEntries The number of non-zero entries that are not on the
	 * diagonal.
	 */
	void initialize(uint_fast64_t nonZeroEntries) {
		// Check whether initializing the matrix is safe.
		if (internalStatus != MatrixStatus::UnInitialized) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::initialize: Throwing invalid state for status flag != 0 (is ", pantheios::integer(internalStatus), " - Already initialized?");
			throw mrmc::exceptions::invalid_state("StaticSparseMatrix::initialize: Invalid state for status flag != 0 - Already initialized?");
		} else if (rowCount == 0) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::initialize: Throwing invalid_argument for row_count = 0");
			throw mrmc::exceptions::invalid_argument("mrmc::StaticSparseMatrix::initialize: Matrix with 0 rows is not reasonable");
		} else if (((rowCount * rowCount) - rowCount) < nonZeroEntries) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::initialize: Throwing invalid_argument: More non-zero entries than entries in target matrix");
			throw mrmc::exceptions::invalid_argument("mrmc::StaticSparseMatrix::initialize: More non-zero entries than entries in target matrix");
		} else {
			// If it is safe, initialize necessary members and prepare the
			// internal storage.
			nonZeroEntryCount = nonZeroEntries;
			lastRow = 0;

			if (!prepareInternalStorage()) {
				triggerErrorState();
				pantheios::log_ERROR("StaticSparseMatrix::initialize: Throwing bad_alloc: memory allocation failed");
				throw std::bad_alloc();
			} else {
				setState(MatrixStatus::Initialized);
			}
		}
	}

	/*!
	 * Initializes the sparse matrix with the given Eigen sparse matrix.
	 * NOTE: Calling this method before any other member function is mandatory.
	 * This version is only to be used when copying an Eigen sparse matrix. For
	 * initialization with addNextValue() and finalize() use initialize(uint_fast32_t)
	 * instead.
	 * @param eigenSparseMatrix The Eigen sparse matrix to be copied.
	 * *NOTE* Has to be in compressed form!
	 */
	template<int _Options, typename _Index>
	void initialize(const Eigen::SparseMatrix<T, _Options, _Index>& eigenSparseMatrix) {
		// Throw an error in case the matrix is not in compressed format.
		if (!eigenSparseMatrix.isCompressed()) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::initialize: Throwing invalid_argument: eigen_sparse_matrix is not in Compressed form.");
			throw mrmc::exceptions::invalid_argument("StaticSparseMatrix::initialize: Throwing invalid_argument: eigen_sparse_matrix is not in Compressed form.");
		}

		// Compute the actual (i.e. non-diagonal) number of non-zero entries.
		nonZeroEntryCount = getEigenSparseMatrixCorrectNonZeroEntryCount(eigenSparseMatrix);
		lastRow = 0;

		// Try to prepare the internal storage and throw an error in case of
		// failure.
		if (!prepareInternalStorage()) {
			triggerErrorState();
			pantheios::log_ERROR(
					"StaticSparseMatrix::initialize: Throwing bad_alloc: memory allocation failed");
			throw std::bad_alloc();
		} else {
			// Get necessary pointers to the contents of the Eigen matrix.
			const T* valuePtr = eigenSparseMatrix.valuePtr();
			const _Index* indexPtr = eigenSparseMatrix.innerIndexPtr();
			const _Index* outerPtr = eigenSparseMatrix.outerIndexPtr();

			// If the given matrix is in RowMajor format, copying can simply
			// be done by adding all values in order.
			// Direct copying is, however, prevented because we have to
			// separate the diagonal entries from others.
			if (isEigenRowMajor(eigenSparseMatrix)) {
				// Because of the RowMajor format outerSize evaluates to the
				// number of rows.
				const _Index rowCount = eigenSparseMatrix.outerSize();
				for (_Index row = 0; row < rowCount; ++row) {
					for (_Index col = outerPtr[row]; col < outerPtr[row + 1]; ++col) {
						addNextValue(row, indexPtr[col], valuePtr[col]);
					}
				}
			} else {
				const _Index entryCount = eigenSparseMatrix.nonZeros();
				// Because of the ColMajor format outerSize evaluates to the
				// number of columns.
				const _Index colCount = eigenSparseMatrix.outerSize();

				// Create an array to remember which elements have to still
				// be searched in each column and initialize it with the starting
				// index for every column.
				_Index* positions = new _Index[colCount]();
				for (_Index i = 0; i < colCount; ++i) {
					positions[i] = outerPtr[i];
				}

				// Now copy the elements. As the matrix is in ColMajor format,
				// we need to iterate over the columns to find the next non-zero
				// entry.
				int i = 0;
				int currentRow = 0;
				int currentColumn = 0;
				while (i < entryCount) {
					// If the current element belongs the the current column,
					// add it in case it is also in the current row.
					if ((positions[currentColumn] < outerPtr[currentColumn + 1])
							&& (indexPtr[positions[currentColumn]] == currentRow)) {
						addNextValue(currentRow, currentColumn,
								valuePtr[positions[currentColumn]]);
						// Remember that we found one more non-zero element.
						++i;
						// Mark this position as "used".
						++positions[currentColumn];
					}

					// Now we can advance to the next column and also row,
					// in case we just iterated through the last column.
					++currentColumn;
					if (currentColumn == colCount) {
						currentColumn = 0;
						++currentRow;
					}
				}
				delete[] positions;
			}
			setState(MatrixStatus::Initialized);
		}
	}

	/*!
	 * Sets the matrix element at the given row and column to the given value.
	 * NOTE: This is a linear setter. It must be called consecutively for each element,
	 * row by row *and* column by column. Only diagonal entries may be set at any time.
	 * @param row The row in which the matrix element is to be set.
	 * @param col The column in which the matrix element is to be set.
	 * @param value The value that is to be set.
	 */
	void addNextValue(const uint_fast64_t row, const uint_fast64_t col,	const T& value) {
		// Check whether the given row and column positions are valid and throw
		// error otherwise.
		if ((row > rowCount) || (col > rowCount)) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::addNextValue: Throwing out_of_range: row or col not in 0 .. rows (is ",
					pantheios::integer(row), " x ", pantheios::integer(col), ", max is ",
					pantheios::integer(rowCount), " x ", pantheios::integer(rowCount), ").");
			throw mrmc::exceptions::out_of_range("StaticSparseMatrix::addNextValue: row or col not in 0 .. rows");
		}

		if (row == col) { // Set a diagonal element.
			diagonalStorage[row] = value;
		} else { // Set a non-diagonal element.
			// If we switched to another row, we have to adjust the missing
			// entries in the row_indications array.
			if (row != lastRow) {
				for (uint_fast64_t i = lastRow + 1; i <= row; ++i) {
					rowIndications[i] = currentSize;
				}
				lastRow = row;
			}

			// Finally, set the element and increase the current size.
			valueStorage[currentSize] = value;
			columnIndications[currentSize] = col;

			++currentSize;
		}
	}

	/*
	 * Finalizes the sparse matrix to indicate that initialization has been
	 * completed and the matrix may now be used.
	 */
	void finalize() {
		// Check whether it's safe to finalize the matrix and throw error
		// otherwise.
		if (!isInitialized()) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::finalize: Throwing invalid state for internal state not Initialized (is ",
					pantheios::integer(internalStatus), " - Already finalized?");
			throw mrmc::exceptions::invalid_state("StaticSparseMatrix::finalize: Invalid state for internal state not Initialized - Already finalized?");
		} else if (currentSize != nonZeroEntryCount) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::finalize: Throwing invalid_state: Wrong call count for addNextValue");
			throw mrmc::exceptions::invalid_state("StaticSparseMatrix::finalize: Wrong call count for addNextValue");
		} else {
			// Fill in the missing entries in the row_indications array.
			// (Can happen because of empty rows at the end.)
			if (lastRow != rowCount) {
				for (uint_fast64_t i = lastRow + 1; i <= rowCount; ++i) {
					rowIndications[i] = currentSize;
				}
			}

			// Set a sentinel element at the last position of the row_indications
			// array. This eases iteration work, as now the indices of row i
			// are always between row_indications[i] and row_indications[i + 1],
			// also for the first and last row.
			rowIndications[rowCount + 1] = nonZeroEntryCount;

			setState(MatrixStatus::ReadReady);
		}
	}

	/*!
	 * Gets the matrix element at the given row and column to the given value.
	 * NOTE: This function does not check the internal status for errors for performance reasons.
	 * @param row The row in which the element is to be read.
	 * @param col The column in which the element is to be read.
	 * @param target A pointer to the memory location where the read content is
	 * to be put.
	 * @return True iff the value is set in the matrix, false otherwise.
	 * On false, 0 will be written to *target.
	 */
	inline bool getValue(uint_fast64_t row, uint_fast64_t col, T* const target) {
		// Check for illegal access indices.
		if ((row > rowCount) || (col > rowCount)) {
			pantheios::log_ERROR("StaticSparseMatrix::getValue: row or col not in 0 .. rows (is ", pantheios::integer(row), " x ",
					pantheios::integer(col), ", max is ", pantheios::integer(rowCount), " x ",	pantheios::integer(rowCount), ").");
			throw mrmc::exceptions::out_of_range("StaticSparseMatrix::getValue: row or col not in 0 .. rows");
			return false;
		}

		// Read elements on the diagonal directly.
		if (row == col) {
			*target = diagonalStorage[row];
			return true;
		}

		// In case the element is not on the diagonal, we have to iterate
		// over the accessed row to find the element.
		uint_fast64_t rowStart = rowIndications[row];
		uint_fast64_t rowEnd = rowIndications[row + 1];
		while (rowStart < rowEnd) {
			// If the lement is found, write the content to the specified
			// position and return true.
			if (columnIndications[rowStart] == col) {
				*target = valueStorage[rowStart];
				return true;
			}
			// If the column of the current element is already larger than the
			// requested column, the requested element cannot be contained
			// in the matrix and we may therefore stop searching.
			if (columnIndications[rowStart] > col) {
				break;
			}
			++rowStart;
		}

		// Set 0 as the content and return false in case the element was not found.
		*target = 0;
		return false;
	}

	/*!
	 * Returns the number of rows of the matrix.
	 */
	uint_fast64_t getRowCount() const {
		return rowCount;
	}

	/*!
	 * Returns a pointer to the value storage of the matrix. This storage does
	 * *not* include elements on the diagonal.
	 * @return A pointer to the value storage of the matrix.
	 */
	T* getStoragePointer() const {
		return valueStorage;
	}

	/*!
	 * Returns a pointer to the storage of elements on the diagonal.
	 * @return A pointer to the storage of elements on the diagonal.
	 */
	T* getDiagonalStoragePointer() const {
		return diagonalStorage;
	}

	/*!
	 * Returns a pointer to the array that stores the start indices of non-zero
	 * entries in the value storage for each row.
	 * @return A pointer to the array that stores the start indices of non-zero
	 * entries in the value storage for each row.
	 */
	uint_fast64_t* getRowIndicationsPointer() const {
		return rowIndications;
	}

	/*!
	 * Returns a pointer to an array that stores the column of each non-zero
	 * element that is not on the diagonal.
	 * @return A pointer to an array that stores the column of each non-zero
	 * element that is not on the diagonal.
	 */
	uint_fast64_t* getColumnIndicationsPointer() const {
		return columnIndications;
	}

	/*!
	 * Checks whether the internal status of the matrix makes it ready for
	 * reading access.
	 * @return True iff the internal status of the matrix makes it ready for
	 * reading access.
	 */
	bool isReadReady() {
		return (internalStatus == MatrixStatus::ReadReady);
	}

	/*!
	 * Checks whether the matrix was initialized previously. The matrix may
	 * still require to be finalized, even if this check returns true.
	 * @return True iff the matrix was initialized previously.
	 */
	bool isInitialized() {
		return (internalStatus == MatrixStatus::Initialized || internalStatus == MatrixStatus::ReadReady);
	}

	/*!
	 * Returns the internal state of the matrix.
	 * @return The internal state of the matrix.
	 */
	MatrixStatus getState() {
		return internalStatus;
	}

	/*!
	 * Checks whether the internal state of the matrix signals an error.
	 * @return True iff the internal state of the matrix signals an error.
	 */
	bool hasError() {
		return (internalStatus == MatrixStatus::Error);
	}

	/*!
	 * Exports this sparse matrix to Eigens sparse matrix format.
	 * NOTE: this requires this matrix to be in the ReadReady state.
	 * @return The sparse matrix in Eigen format.
	 */
	Eigen::SparseMatrix<T, Eigen::RowMajor, int_fast32_t>* toEigenSparseMatrix() {
		// Check whether it is safe to export this matrix.
		if (!isReadReady()) {
			triggerErrorState();
			pantheios::log_ERROR("StaticSparseMatrix::toEigenSparseMatrix: Throwing invalid state for internal state not ReadReady (is ", pantheios::integer(internalStatus), ").");
			throw mrmc::exceptions::invalid_state("StaticSparseMatrix::toEigenSparseMatrix: Invalid state for internal state not ReadReady.");
		} else {
			// Create a
			int_fast32_t eigenRows = static_cast<int_fast32_t>(rowCount);
			Eigen::SparseMatrix<T, Eigen::RowMajor, int_fast32_t>* mat = new Eigen::SparseMatrix<T, Eigen::RowMajor, int_fast32_t>(eigenRows, eigenRows);

			// There are two ways of converting this matrix to Eigen's format.
			// 1. Compute a list of triplets (row, column, value) for all
			// non-zero elements and pass it to Eigen to create a sparse matrix.
			// 2. Tell Eigen to reserve the average number of non-zero elements
			// per row in advance and insert the values via a call to Eigen's
			// insert method then. As the given reservation number is only an
			// estimate, the actual number may be different and Eigen may have
			// to shift a lot.
			// In most cases, the second alternative is faster (about 1/2 of the
			// first, but if there are "heavy" rows that are several times larger
			// than an average row, the other solution might be faster.
			// The desired conversion method may be set by an appropriate define.

#			ifdef MRMC_USE_TRIPLETCONVERT

			// FIXME: Wouldn't it be more efficient to add the elements in
			// order including the diagonal elements? Otherwise, Eigen has to
			// perform some sorting.

			// Prepare the triplet storage.
			typedef Eigen::Triplet<T> IntTriplet;
			std::vector<IntTriplet> tripletList;
			tripletList.reserve(nonZeroEntryCount + rowCount);

			// First, iterate over all elements that are not on the diagonal
			// and add the corresponding triplet.
			uint_fast64_t rowStart;
			uint_fast64_t rowEnd;
			for (uint_fast64_t row = 0; row <= rowCount; ++row) {
				rowStart = rowIndications[row];
				rowEnd = rowIndications[row + 1];
				while (rowStart < rowEnd) {
					tripletList.push_back(IntTriplet(row, columnIndications[rowStart], valueStorage[rowStart]));
					++rowStart;
				}
			}

			// Then add the elements on the diagonal.
			for (uint_fast64_t i = 0; i <= rowCount; ++i) {
				tripletList.push_back(IntTriplet(i, i, diagonalStorage[i]));
			}

			// Let Eigen create a matrix from the given list of triplets.
			mat->setFromTriplets(tripletList.begin(), tripletList.end());

#			else // NOT MRMC_USE_TRIPLETCONVERT

			// Reserve the average number of non-zero elements per row for each
			// row.
			mat->reserve(Eigen::VectorXi::Constant(eigenRows, static_cast<int_fast32_t>((nonZeroEntryCount + rowCount) / eigenRows)));

			// Iterate over the all non-zero elements in this matrix and add
			// them to the matrix individually.
			uint_fast64_t rowStart;
			uint_fast64_t rowEnd;
			for (uint_fast64_t row = 0; row <= rowCount; ++row) {
				rowStart = rowIndications[row];
				rowEnd = rowIndications[row + 1];

				// Insert the element on the diagonal.
				mat->insert(row, row) = diagonalStorage[row];

				// Insert the elements that are not on the diagonal
				while (rowStart < rowEnd) {
					mat->insert(row, columnIndications[rowStart]) = valueStorage[rowStart];
					++rowStart;
				}
			}
#			endif // MRMC_USE_TRIPLETCONVERT

			// Make the matrix compressed, i.e. remove remaining zero-entries.
			mat->makeCompressed();

			return mat;
		}

		// This point can never be reached as both if-branches end in a return
		// statement.
		return nullptr;
	}

	/*!
	 * Returns the number of non-zero entries that are not on the diagonal.
	 * @returns The number of non-zero entries that are not on the diagonal.
	 */
	uint_fast64_t getNonZeroEntryCount() const {
		return nonZeroEntryCount;
	}

	/*!
	 * This function makes the given state absorbing. This means that all
	 * entries in its row will be changed to 0 and the value 1 will be written
	 * to the element on the diagonal.
	 * @param state The state to be made absorbing.
	 * @returns True iff the operation was successful.
	 */
	bool makeStateAbsorbing(const uint_fast64_t state) {
		// Check whether the accessed state exists.
		if (state > rowCount) {
			pantheios::log_ERROR("StaticSparseMatrix::makeStateFinal: state not in 0 .. rows (is ",	pantheios::integer(state), ", max is ",	pantheios::integer(rowCount), ").");
			throw mrmc::exceptions::out_of_range("StaticSparseMatrix::makeStateFinal: state not in 0 .. rows");
			return false;
		}

		// Iterate over the elements in the row that are not on the diagonal
		// and set them to zero.
		uint_fast64_t rowStart = rowIndications[state];
		uint_fast64_t rowEnd = rowIndications[state + 1];

		while (rowStart < rowEnd) {
			valueStorage[rowStart] = mrmc::misc::constGetZero(valueStorage);
			++rowStart;
		}

		// Set the element on the diagonal to one.
		diagonalStorage[state] = mrmc::misc::constGetOne(diagonalStorage);
		return true;
	}

	/*!
	 * Returns the size of the matrix in memory measured in bytes.
	 * @return The size of the matrix in memory measured in bytes.
	 */
	uint_fast64_t getSizeInMemory() {
		uint_fast64_t size = sizeof(*this);
		// Add value_storage size.
		size += sizeof(T) * nonZeroEntryCount;
		// Add diagonal_storage size.
		size += sizeof(T) * (rowCount + 1);
		// Add column_indications size.
		size += sizeof(uint_fast64_t) * nonZeroEntryCount;
		// Add row_indications size.
		size += sizeof(uint_fast64_t) * (rowCount + 1);
		return size;
	}

private:

	/*!
	 * The number of rows of the matrix.
	 */
	uint_fast64_t rowCount;

	/*!
	 * The number of non-zero elements that are not on the diagonal.
	 */
	uint_fast64_t nonZeroEntryCount;

	/*!
	 * Stores all non-zero values that are not on the diagonal.
	 */
	T* valueStorage;

	/*!
	 * Stores all elements on the diagonal, even the ones that are zero.
	 */
	T* diagonalStorage;

	/*!
	 * Stores the column for each non-zero element that is not on the diagonal.
	 */
	uint_fast64_t* columnIndications;

	/*!
	 * Array containing the boundaries (indices) in the value_storage array
	 * for each row. All elements of value_storage with indices between the
	 * i-th and the (i+1)-st element of this array belong to row i.
	 */
	uint_fast64_t* rowIndications;

	/*!
	 * The internal status of the matrix.
	 */
	MatrixStatus internalStatus;

	/*!
	 * Stores the current number of non-zero elements that have been added to
	 * the matrix. Used for correctly inserting elements in the matrix.
	 */
	uint_fast64_t currentSize;

	/*!
	 * Stores the row in which the last element was inserted. Used for correctly
	 * inserting elements in the matrix .
	 */
	uint_fast64_t lastRow;

	/*!
	 * Sets the internal status to signal an error.
	 */
	void triggerErrorState() {
		setState(MatrixStatus::Error);
	}

	/*! 
	 * Sets the internal status to the given state if the current state is not
	 * the error state.
	 * @param new_state The new state to be switched to.
	 */
	void setState(const MatrixStatus new_state) {
		internalStatus = (internalStatus == MatrixStatus::Error) ? internalStatus : new_state;
	}

	/*!
	 * Prepares the internal CSR storage. For this, it requires
	 * non_zero_entry_count and row_count to be set correctly.
	 * @return True on success, false otherwise (allocation failed).
	 */
	bool prepareInternalStorage() {
		// Set up the arrays for the elements that are not on the diagonal.
		valueStorage = new (std::nothrow) T[nonZeroEntryCount]();
		columnIndications = new (std::nothrow) uint_fast64_t[nonZeroEntryCount]();

		// Set up the row_indications array and reserve one element more than
		// there are rows in order to put a sentinel element at the end,
		// which eases iteration process.
		rowIndications = new (std::nothrow) uint_fast64_t[rowCount + 1]();

		// Set up the array for the elements on the diagonal.
		diagonalStorage = new (std::nothrow) T[rowCount]();

		// Return whether all the allocations could be made without error.
		return ((valueStorage != NULL) && (columnIndications != NULL)
				&& (rowIndications != NULL) && (diagonalStorage != NULL));
	}

	/*!
	 * Helper function to determine whether the given Eigen matrix is in RowMajor
	 * format. Always returns true, but is overloaded, so the compiler will
	 * only call it in case the Eigen matrix is in RowMajor format.
	 * @return True.
	 */
	template<typename _Scalar, typename _Index>
	bool isEigenRowMajor(Eigen::SparseMatrix<_Scalar, Eigen::RowMajor, _Index>) {
		return true;
	}

	/*!
	 * Helper function to determine whether the given Eigen matrix is in RowMajor
	 * format. Always returns false, but is overloaded, so the compiler will
	 * only call it in case the Eigen matrix is in ColMajor format.
	 * @return False.
	 */
	template<typename _Scalar, typename _Index>
	bool isEigenRowMajor(
			Eigen::SparseMatrix<_Scalar, Eigen::ColMajor, _Index>) {
		return false;
	}

	/*!
	 * Helper function to determine the number of non-zero elements that are
	 * not on the diagonal of the given Eigen matrix.
	 * @param eigen_sparse_matrix The Eigen matrix to analyze.
	 * @return The number of non-zero elements that are not on the diagonal of
	 * the given Eigen matrix.
	 */
	template<typename _Scalar, int _Options, typename _Index>
	_Index getEigenSparseMatrixCorrectNonZeroEntryCount(const Eigen::SparseMatrix<_Scalar, _Options, _Index>& eigen_sparse_matrix) {
		const _Index* indexPtr = eigen_sparse_matrix.innerIndexPtr();
		const _Index* outerPtr = eigen_sparse_matrix.outerIndexPtr();

		const _Index entryCount = eigen_sparse_matrix.nonZeros();
		const _Index outerCount = eigen_sparse_matrix.outerSize();

		uint_fast64_t diagNonZeros = 0;

		// For RowMajor, row is the current row and col the column and for
		// ColMajor, row is the current column and col the row, but this is
		// not important as we are only looking for elements on the diagonal.
		_Index innerStart = 0;
		_Index innerEnd = 0;
		_Index innerMid = 0;
		for (_Index row = 0; row < outerCount; ++row) {
			innerStart = outerPtr[row];
			innerEnd = outerPtr[row + 1] - 1;

			// Now use binary search (but defer equality detection).
			while (innerStart < innerEnd) {
				innerMid = innerStart + ((innerEnd - innerStart) / 2);

				if (indexPtr[innerMid] < row) {
					innerStart = innerMid + 1;
				} else {
					innerEnd = innerMid;
				}
			}

			// Check whether we have found an element on the diagonal.
			if ((innerStart == innerEnd) && (indexPtr[innerStart] == row)) {
				++diagNonZeros;
			}
		}

		return static_cast<_Index>(entryCount - diagNonZeros);
	}

};

} // namespace sparse

} // namespace mrmc

#endif // MRMC_SPARSE_STATIC_SPARSE_MATRIX_H_
