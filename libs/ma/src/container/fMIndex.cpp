/**
 * @file fMIndex.cpp
 * @author Arne Kutzner
 */
#include "ma/container/fMIndex.h"

#include <cstdlib>
using namespace libMA;

#define complement( x ) ( uint8_t ) NucSeq::nucleotideComplement( x )

SAInterval SuffixArrayInterface::extend_backward(
    // current interval
    const SAInterval& ik,
    // the character to extend with
    const uint8_t c )
{
    throw std::runtime_error( "unimplemented!" );
} // method

SAInterval FMIndex::extend_backward(
    // current interval
    const SAInterval& ik,
    // the character to extend with
    const uint8_t c )
{
    if( c >= 4 /* A,C,T,G */ )
        return SAInterval( 0, 0, 0 ); // return an empty interval

    bwt64bitCounter cntk[ 4 ]; // Number of A, C, G, T in BWT until start of interval ik
    bwt64bitCounter cntl[ 4 ]; // Number of A, C, G, T in BWT until end of interval ik

    assert( ik.start( ) < ik.end( ) );
    assert( ik.start( ) > 0 );

    // here the intervals seem to be (a,b] while mine are [a,b)
    bwt_2occ4(
        // start of SA index interval
        ik.start( ) - 1,
        // end of SA index interval
        ik.end( ) - 1,
        cntk, // output: Number of A, C, G, T until start of interval
        cntl // output: Number of A, C, G, T until end of interval
    );

    DEBUG( for( unsigned int i = 0; i < 4; i++ ) assert( cntk[ i ] <= cntl[ i ] ); ) // DEBUG

    bwt64bitCounter cnts[ 4 ]; // Number of A, C, G, T in BWT interval ik
    // the cnts calculated here might be off by one
    for( unsigned int i = 0; i < 4; i++ )
        cnts[ i ] = cntl[ i ] - cntk[ i ];

    DEBUG_3( std::cout << cnts[ 0 ] << " + " << cnts[ 1 ] << " + " << cnts[ 2 ] << " + " << cnts[ 3 ] << " = "
                       << ( t_bwtIndex )( cnts[ 0 ] + cnts[ 1 ] + cnts[ 2 ] + cnts[ 3 ] ) << " ?= " << ik.size( )
                       << "(-1)" << std::endl; )

    bwt64bitCounter cntk_2[ 4 ];
    cntk_2[ 0 ] = ik.startRevComp( );
    /*
     * PROBLEM:
     *
     * the representation of the $ in the count part of the FM_index is indirect
     *         done by storing the position of the $
     * if have two bwt indices k and l
     * the counts do not return the $ obviously...
     *
     * The result may be off by one since sometimes we have a $ before the current pos
     * sometimes we do not...
     *
     * lets adjust the sizes of the smaller intervals accordingly
     */
    if( ik.start( ) <= primary && ik.end( ) > primary )
    {
        cntk_2[ 0 ]++;
        DEBUG_2( std::cout << "adjusted cntk_2[0] because of primary" << std::endl; )
        assert( ( t_bwtIndex )( cnts[ 0 ] + cnts[ 1 ] + cnts[ 2 ] + cnts[ 3 ] ) == ik.size( ) - 1 );
    } // if
    else
    {
        if( ( t_bwtIndex )( cnts[ 0 ] + cnts[ 1 ] + cnts[ 2 ] + cnts[ 3 ] ) != ik.size( ) )
        {
            std::cout << ik.start( ) << " " << ik.end( ) << " " << primary << std::endl;
            std::cout << cnts[ 0 ] << " + " << cnts[ 1 ] << " + " << cnts[ 2 ] << " + " << cnts[ 3 ] << " = "
                      << ( t_bwtIndex )( cnts[ 0 ] + cnts[ 1 ] + cnts[ 2 ] + cnts[ 3 ] ) << " ?= " << ik.size( )
                      << "(-1)" << std::endl;
        } // if
        assert( ( t_bwtIndex )( cnts[ 0 ] + cnts[ 1 ] + cnts[ 2 ] + cnts[ 3 ] ) == ik.size( ) );
    } // else
    // for all nucleotides
    for( unsigned int i = 1; i < 4; i++ )
        cntk_2[ i ] = cntk_2[ i - 1 ] + cnts[ complement( i - 1 ) ];

    assert( cnts[ c ] >= 0 );


    // BWAs SA intervals seem to be (a,b] while mine are [a,b)
    // pFM_index->L2[c] start of nuc c in BWT
    // cntk[c] offset of new interval
    // cntl[c] end of new interval
    return SAInterval( L2[ c ] + cntk[ c ] + 1, cntk_2[ complement( c ) ], cnts[ c ] );
} // method


SAInterval FMIndex::getInterval( std::shared_ptr<NucSeq> pQuerySeq )
{
    // query sequence itself
    const uint8_t* q = pQuerySeq->pGetSequenceRef( );

    size_t i = pQuerySeq->length( ) - 1;
    SAInterval ik( L2[ (int)q[ i ] ] + 1, L2[ complement( q[ i ] ) ] + 1, L2[ (int)q[ i ] + 1 ] - L2[ (int)q[ i ] ] );
    while( i > 0 && ik.size( ) > 0 )
        ik = extend_backward( ik, q[ --i ] );
    return ik;
} // function

t_bwtIndex FMIndex::get_ambiguity( std::shared_ptr<NucSeq> pQuerySeq )
{
    return getInterval( pQuerySeq ).size( );
} // function

bool FMIndex::testSaInterval( std::shared_ptr<NucSeq> pQuerySeq, const Pack& rPack )
{
    SAInterval ik = getInterval( pQuerySeq );
    for( auto ulCurrPos = ik.start( ); ulCurrPos < ik.end( ); ulCurrPos++ )
    {
        // calculate the referenceIndex using pxUsedFmIndex->bwt_sa() and call fDo for every match
        // individually
        bwtint_t ulIndexOnRefSeq = bwt_sa( ulCurrPos );
        int64_t iDummy;
        if( !rPack.bridgingSubsection( ulIndexOnRefSeq, ulIndexOnRefSeq + pQuerySeq->length( ), iDummy ) &&
            !rPack.vExtract( ulIndexOnRefSeq, ulIndexOnRefSeq + pQuerySeq->length( ) )->equal( *pQuerySeq ) )
            return false;
    } // for

    return true;
} // fuction


bool FMIndex::test( const Pack& rPack, unsigned int uiNumTest )
{
    while( uiNumTest-- > 0 )
    {
        int64_t iDummy;
        auto uiPos = std::rand( ) % rPack.uiUnpackedSizeForwardPlusReverse( );
        if( !rPack.bridgingSubsection( uiPos, uiPos + 10, iDummy ) &&
            !testSaInterval( rPack.vExtract( uiPos, uiPos + 10 ), rPack ) )
            return false;
    } // while
    return true;
} // function

void FMIndex::bwt_pac2bwt_step1( const NucSeq& fn_pac )
{
    /* Size of the reference sequence
     */
    this->uiRefSeqLength = fn_pac.length( ); // bwt->seq_len = bwa_seq_len( fn_pac );

    /* The sequence length is doubled if we have the reverse sequence in th BWT.
     */
    // uiRefSeqLength = uiRefSeqLength; // FIXED: unnecessary self assignment

    /* Buffer for BWT construction. The BWT will be finally inside the buffer.
     */
    std::unique_ptr<uint8_t[]> buf(
        new uint8_t[ uiRefSeqLength + 1 ] ); // original code: buf = (ubyte_t*)calloc( bwt->seq_len + 1, 1 );

    /* Initialize the buffer with the reference sequence and prepare the cumulative count.
     */
    for( uint64_t i = 0; i < ( uiRefSeqLength ); ++i )
    {
        buf[ i ] = fn_pac[ i ]; // buf2[i >> 2] >> ((3 - (i & 3)) << 1) & 3;
        ++L2[ 1 + buf[ i ] ];
    } // for

    /* Complete cumulative count
     */
    for( int i = 2; i <= 4; ++i )
    {
        L2[ i ] += L2[ i - 1 ];
    } // for

    /* Burrows-Wheeler Transform
     * Here we call external code (in is.c) for BWT construction.
     * The BWT construction happens in-place in buf. (This is why we allocate one byte
     * additionally.)
     */
    primary = is_bwt( buf.get( ), (int)uiRefSeqLength );

    /* Compute the expected size of the bwt expressed in 32 bit blocks and resize the bwt vector
     * appropriately.
     */
    auto uiExpectedBWTSize = ( uiRefSeqLength + 15 ) >> 4;
    bwt.resize( uiExpectedBWTSize );
    /* Copy bwt from buffer into vector.
     * FIX ME: The construction programs should already work with the buffer.
     */
    for( uint64_t i = 0; i < uiRefSeqLength; ++i )
    { /* Byte to packed transformation.
       */
        bwt[ i >> 4 ] |= buf[ i ] << ( ( 15 - ( i & 15 ) ) << 1 );
    } // for
} // method

void FMIndex::bwt_bwtupdate_core_step2( )
{
    bwtint_t i, k, c[ 4 ], n_occ;

    /* Number of occurrence counter intervals.
     * Normally OCC_INTERVAL is 0x80 = 128, so we count the symbols for blocks of 128 symbols.
     */
    n_occ = ( uiRefSeqLength + OCC_INTERVAL - 1 ) / OCC_INTERVAL + 1;

    /* Create a temporary vector for storage of the BWT with injected Occ-counting blocks.
     */
    auto uiOccInjectedBWTExpectedSize = bwt.size( ) + n_occ * sizeof( bwtint_t );
    std::vector<uint32_t> xVectorWithOccInjections( uiOccInjectedBWTExpectedSize );

    c[ 0 ] = c[ 1 ] = c[ 2 ] = c[ 3 ] = 0; // temporary uint_64 counter for A, C, G, T

    /* We iterate over the existing BWT and insert each 128th element.
     * k counts in 16 nucleotides.
     * i iterates over the nucleotides.
     * FIX ME: In the context of STL vectors the memcpy is not that nice anymore ...
     */
    for( i = k = 0; i < static_cast<bwtint_t>( uiRefSeqLength ); ++i )
    {
        if( i % OCC_INTERVAL == 0 )
        {
            memcpy( &xVectorWithOccInjections[ 0 ] + k, c,
                    sizeof( bwtint_t ) * 4 ); // set the 4 counter initially to the current value of c

            /* Increment k according to the required size. 4 * sizeof(bwtint_t) many bytes required.
             * k counts in 4 bytes steps. ( in fact: sizeof(bwtint_t)=4*(sizeof(bwtint_t)/4) )
             */
            k += sizeof( bwtint_t );
        } // if

        /* i iterates over the nucleotides, so each 16 nucleotides we have a uint32_t for copying.
         */
        if( i % 16 == 0 )
        {
            xVectorWithOccInjections[ k++ ] = bwt[ i / 16 ]; // 16 == sizeof(uint32_t)/2
        } // if

        /* We extract the nucleotide (2 bits) on position i out of corresponding 16 nucleotide block
         * and increment the correct counter for the symbol.
         * ++c[ bwt->bwt[i >> 4] >>((~i & 0xf)<<1)&3 ];
         */
        //// std::cout << bwt_B00( i ); (here we can pick up the BWT elements in the context of
        /// debugging before inserting the Counting Blocks)
        ++c[ bwt_B00( i ) ];
    } // for
    ////std::cout << "\n";

    /* Save the last counter-block (element).
     * Check whether everything is fine.
     */
    memcpy( &xVectorWithOccInjections[ 0 ] + k, c, sizeof( bwtint_t ) * 4 );
    assert( k + sizeof( bwtint_t ) == xVectorWithOccInjections.size( ) ); //  "inconsistent bwt_size"

    /* Move the bwt with injections to the bwt of the FM-index.
     */
    bwt = std::move( xVectorWithOccInjections );
} // method

void FMIndex::bwt_cal_sa_step3( unsigned int intv )
{
    assert( ispowerof2( intv ) ); //, "SA sample interval is not a power of 2.");

    // if (bwt->sa) free(bwt->sa);
    sa_intv = intv;

    /* FIX ME: resize initializes all elements with zero. Better would be reserve and later vector
     * insertion
     */
    size_t uiRequiredSAsize = ( uiRefSeqLength + intv ) / intv;
    sa.resize( uiRequiredSAsize );

    /* calculate SA values
     */
    bwtint_t isa = 0; // "jumping" index position
    bwtint_t sa = uiRefSeqLength; // S(isa) = sa (sa at jumping index position); counts down to 1

    /* Implements an iteration scheme that counts over all suffix array elements.
     * Basic idea BWT based suffix array reconstruction.
     * Important observation: The suffix array can be always reconstructed using the BWT, because
     * indirectly the BWT contains all info.
     */
    for( bwtint_t i = 0; i < static_cast<bwtint_t>( uiRefSeqLength ); ++i )
    {
        /* Do we have an interval position?
         */
        if( isa % intv == 0 )
        {
            this->sa[ isa / intv ] = sa;
        } // if
        --sa;

        /* Core iteration step
         */
        isa = bwt_invPsi( isa );

        assert( isa >= 0 );
    } // for

    /* Do not forget the last block, if necessary.
     */
    if( isa % intv == 0 )
    {
        this->sa[ isa / intv ] = sa;
    } // if

    this->sa[ 0 ] = (bwtint_t)-1; // before this line, bwt->sa[0] = bwt->seq_len
} // method

void FMIndex::build_FMIndex( const Pack& rxSequenceCollection, // the pack for which we compute a BWT
                             unsigned int uiAlgorithmSelection )
{
    if( uiAlgorithmSelection > 1 )
    {
        uiAlgorithmSelection = rxSequenceCollection.uiUnpackedSizeForwardPlusReverse( ) < 50000000
                                   ? 0
                                   : 1; // automatic selection depending on size of pack
    } // if

    /*
     * Step 1: Create the basic BWT.
     */
    if( uiAlgorithmSelection == 0 )
    {
        /*
         * For small packs we transform the pack into a single sequence
         * with reverse strand and apply the algorithm for small inputs.
         */
        auto pSequence =
            rxSequenceCollection.vColletionAsNucSeq( ); // unpack the pack into a single nucleotide sequence
        bwt_pac2bwt_step1( *pSequence ); // construct with build in function
    } // if
    else
    {
        /*
         * In this case we build the BWT using the BWA C-code for large inputs.
         * For delivering the pack to the BWA code we have to rely on an
         * external file currently.
         * FIX ME: The current solution is quite inefficient with respect to memory aspects.
         */

        // set seq_len, so that it represents the size of forward plus reverse strand
        uiRefSeqLength = rxSequenceCollection.uiUnpackedSizeForwardPlusReverse( );

        std::string tempDir = ".tempdir";
        // Check existence / create directory for storage of temporary data.
        makeDir( tempDir );

        // Create temporary filename for pack export.
        const auto xTempFileName = tempDir.append(
            std::to_string(
                std::chrono::system_clock::to_time_t( std::chrono::system_clock::now( ) ) ) // current time as string
                .append( "-" )
                .append(
                    std::to_string( rxSequenceCollection.uiUnpackedSizeForwardPlusReverse( ) ) ) // pack size as string
        ); // temporary filename construction
        // pac file construction adds a suffix
        const auto sTempFileNameWithSuffix = std::string( xTempFileName ).append( ".pac" );

        /* Store the dynamically created pack on the filesystem.
         * The suffix .pac will be automatically added.
         */
        rxSequenceCollection.vCreateAndStorePackForBWTProcessing( xTempFileName );

        /* Start the BWT construction.
         */
        auto xBWTAsTriple = bwtLarge( sTempFileNameWithSuffix.c_str( ) ); // construct the BWT using the old BWA code.

        /* Move the externally constructed BWT to this BWT-object.
         */
        bwt = std::move( std::get<3>( xBWTAsTriple ) );

        this->primary = std::get<1>( xBWTAsTriple ); // copy the primary
        std::copy_n( std::get<2>( xBWTAsTriple ).begin( ), 4,
                     L2.begin( ) + 1 ); // copy the 4 cumulative counters

        /* Remove the temporary file, because it is not needed any longer.
         */
        std::remove( sTempFileNameWithSuffix.c_str( ) );
    } // else

    /* Step 2 and step 3 of FM-index creation
     */
    vPostProcessBWTAndCreateSA( );
} // method

#ifdef WITH_PYTHON


void exportFM_index( libMS::SubmoduleOrganizer& xOrganizer )
{
    // export the SAInterval class
    py::class_<SAInterval>( xOrganizer.util(), "SAInterval" )
        .def_readwrite( "size", &SAInterval::iSize )
        .def_readwrite( "start", &SAInterval::iStart )
        .def( "rev_comp", &SAInterval::revComp );

    py::class_<SuffixArrayInterface, libMS::Container, std::shared_ptr<SuffixArrayInterface>>( xOrganizer._container(),
                                                                                               "SuffixArrayInterface" );

    // export the FM_index class
    py::class_<FMIndex, SuffixArrayInterface, std::shared_ptr<FMIndex>>( xOrganizer.container(), "FMIndex" )
        .def( py::init<>( ) ) // default constructor
        .def( py::init<std::shared_ptr<NucSeq>>( ) )
        .def( py::init<std::shared_ptr<Pack>>( ) )
        .def( "load", &FMIndex::vLoadFMIndex )
        .def_static( "exists", &FMIndex::packExistsOnFileSystem )
        .def( "store", &FMIndex::vStoreFMIndex )
        .def( "init_interval", &FMIndex::init_interval )
        .def( "extend_backward", &FMIndex::extend_backward )
        .def( "bwt_sa", &FMIndex::bwt_sa )
        .def( "get_ambiguity", &FMIndex::get_ambiguity )
        .def( "test_sa_interval", &FMIndex::testSaInterval )
        .def( "__len__", &FMIndex::getRefSeqLength )
        .def( "bwt_2occ4", &FMIndex::bwt_2occ4 );

    // tell boost python that pointers of these classes can be converted implicitly
    py::implicitly_convertible<FMIndex, libMS::Container>( );
} // function
#endif