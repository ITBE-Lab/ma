/* Authors: Arne Kutzner and Markus Schmidt
 * Created: Jan. 2020
 * This file is part of the ITBE-Lab code collection.
 * MIT License
 * @file db_con_pool.h
 * @brief Connection pool for databases
 */

#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "common.h" // general API for SQL like DB

// Rename: PooledSQLDBCon => PooledSQLDBCon
template <typename DBImpl> class PooledSQLDBCon : public SQLDB<DBImpl>
{
  private:
    std::shared_ptr<std::mutex> pPoolLock; // the pool lock is shared with the pool and all its connections

  public:
    size_t uiTaskId; // id of the task belonging to the PooledSQLDBCon
    PooledSQLDBCon( const PooledSQLDBCon<DBImpl>& ) = delete; // no copies of pooled connections

    PooledSQLDBCon( std::shared_ptr<std::mutex> pPoolLock, // pass shared pointer to pool mutex
                    const std::string& rsDBName = "" ) // FIXME: Pass JSON here

        : SQLDB<DBImpl>( rsDBName ), pPoolLock( pPoolLock )
    {} // constructor

    /** @brief The function func is executed protected by a global lock.
     */
    template <typename F> void doPoolSafe( F&& func )
    {
        std::lock_guard<std::mutex> lock( *pPoolLock );
        func( );
    } // method

    inline size_t getTaskId( )
    {
        return this->uiTaskId;
    } // method

}; // class


/* Basic characteristics:
 * - The pool synchronizes threads that ask for access, but the pool itself does not comprises threads.
 *   (Therefore, e.g. an additional thread pool is required for delivering multiple thread)
 * Important notice: If a pooled task throws an exception, this exception belongs to the future and must be caught via
 * the future. For more details see:
 * https://stackoverflow.com/questions/16344663/is-there-a-packaged-taskset-exception-equivalent
 *
 */
template <typename DBImpl> class SQLDBConPool
{
  public:
    // class PooledSQLDBCon
    // {
    //   private:
    //     size_t uiTaskId; // id of the task belonging to the PooledSQLDBCon
    //
    //   public:
    //     PooledSQLDBCon( const PooledSQLDBCon& ) = delete; // no copies of connection managers
    //     std::shared_ptr<SQLDB<DBImpl>> pDBCon;
    //
    //     /** @brief Constructs a DB connection manager. */
    //     PooledSQLDBCon( ) : pDBCon( std::make_shared<PooledSQLDBCon<DBImpl>>( ) )
    //     {
    //         std::cout << "Create PooledSQLDBCon " << std::endl;
    //     } // constructor
    //
    //     inline size_t getTaskId( )
    //     {
    //         return this->uiTaskId;
    //     } // method
    //
    //     friend class SQLDBConPool<DBImpl>;
    // }; // class

  private:
    using TaskType = std::function<void( std::shared_ptr<PooledSQLDBCon<DBImpl>> )>; // type of the tasks to be executed

    std::vector<std::thread> vWorkers; // worker threads; each thread manages one connection
    std::vector<std::shared_ptr<PooledSQLDBCon<DBImpl>>> vConPool; // pointers to actual connections
    std::queue<TaskType> qTasks; // queue of lambda functions that shall to executed
    std::mutex xQueueMutex; // mutex for control of concurrent access of the tasks queue
    std::condition_variable xCondition; // For wait, notify synchronization purposes.
    std::shared_ptr<std::mutex> pPoolLock; // pointer to mutex synchronizes concurrent activities in the pool
    bool bStop; // Flag for 'indicating end of pool operation'. If true, the pool operation is terminated.

  public:
    const size_t uiPoolSize; // size of the pool

    SQLDBConPool( const SQLDBConPool& ) = delete; // no copies of thread pools

    /* TO DO: Configuration via JSON.
     * Notice: This constructor can throw exceptions. When an exception is thrown from a constructor, the object is not
     * considered instantiated, and therefore its destructor will not be called.
     */
    SQLDBConPool( size_t uiPoolSize = 1, // pass pool size here
                  const std::string& rsDBName = "" ) // FIXME: DB info as JSON
        : pPoolLock( std::make_shared<std::mutex>( ) ), // create the pool lock
          bStop( false ), // stop must be false in the beginning
          uiPoolSize( uiPoolSize )
    {
        // Check size of pool
        if( uiPoolSize == 0 )
            throw std::runtime_error( "SQLDBConPool: The requested pool size must be greater than zero." );
        std::cout << "In constructor before emplace_back ..." << std::endl;
        // Create all connection managers.
        // The creation of the connections has to be done sequentially with MySQL or connection creation can fail.
        for( size_t uiItr = 0; uiItr < uiPoolSize; ++uiItr )
            vConPool.emplace_back( std::make_shared<PooledSQLDBCon<DBImpl>>( pPoolLock, rsDBName ) );

        // Create an initialize all workers.
        for( size_t uiTaskId = 0; uiTaskId < uiPoolSize; ++uiTaskId )
            vWorkers.emplace_back( [ this, uiTaskId ] {
                // Treads are not allowed to throw exceptions or we face crashes.
                // Therefore the swallowing of exceptions.
                doNoExcept(
                    [ & ] {
                        // Get a reference to the connection manager belonging to this thread.
                        auto pConManager = vConPool[ uiTaskId ];
                        pConManager->uiTaskId = uiTaskId; // inform connector about corresponding taskid

                        // The loop continues until the stop flags indicates termination.
                        while( true )
                        {
                            // Synchronization of mutual access to the task queue.
                            // Start of scope of exclusive access for a single thread.
                            std::unique_lock<std::mutex> xLock( this->xQueueMutex );

                            // If there is no indication for termination and if there is nothing to do now,
                            // the lock is released so that some producer can push a fresh task into the queue.
                            // After inserting a fresh task, the producer calls notify_one() for awaking one of the
                            // waiting (sleeping) threads. This can be this thread or another in the pool.
                            while( !this->bStop && this->qTasks.empty( ) )
                            {
                                this->xCondition.wait( xLock );
                                // When we continue here, we have exclusive access once again.
                            } // while

                            // If there is an indication for termination and if there is nothing to do any more, we
                            // terminate the thread by returning.
                            if( this->bStop && this->qTasks.empty( ) )
                            {
                                std::cout << "Thread " << uiTaskId << " terminated" << std::endl;
                                return;
                            } // if

                            // We now have exclusive access to a non-empty task queue.
                            // We pick the front element (front lambda expression) for execution and remove it from the
                            // queue.
                            TaskType fTaskToBeExcuted( this->qTasks.front( ) );
                            this->qTasks.pop( );

                            // Exclusive access to the task queue is not required any longer.
                            xLock.unlock( );

                            // Execute the task delivering its connection manager as argument.
                            // If the task throws an exception, this exception has to be caught via the future.
                            // (See the notice in the description of this class.)
                            fTaskToBeExcuted( pConManager );
                        } // while
                    }, // lambda ( doNoExcept )
                    // doNoExcept info message:
                    "SQLDBConPool:: The worker with Id " + std::to_string( uiTaskId ) +
                        "failed due to an internal error and terminated." );
            } ); // emplace_back
    } // constructor

    /** brief@ Terminates the operation of the connection pool.
     *  Method blocks until all tasks in the queue have been executed.
     */
    void shutdown( )
    {
        {
            std::unique_lock<std::mutex> xLock( this->xQueueMutex );
            if( bStop )
                // Shutdown started already; no action required.
                // Return will release the mutex too ...
                return;
            bStop = true;
        } // end of scope for queue_mutex, so we unlock the mutex

        std::cout << "Pool shutdown started ..." << std::endl;
        // Notify all waiting threads to continue so that they recognize the stop flag and terminate.
        xCondition.notify_all( );

        // Wait until all workers finished their job.
        // (The destructor is blocked until all workers finished their job.)
        std::cout << "WORKER:" << vWorkers.size( ) << std::endl;
        for( size_t uiItr = 0; uiItr < vWorkers.size( ); ++uiItr )
            vWorkers[ uiItr ].join( );

        std::cout << "Pool shutdown finished ..." << std::endl;
    } // method

    /** brief@ The destructor blocks until all workers terminated. */
    ~SQLDBConPool( )
    {
        std::cout << "SQLDBConPool Destructor." << std::endl;
        this->shutdown( );
    } // destructor

    /** @brief Enqueue a function func for execution via the DB pool */
    template <class F, class... Args>
    auto inline enqueue( F&& func, Args&&... args )
        -> std::future<typename std::result_of<F( std::shared_ptr<PooledSQLDBCon<DBImpl>>, Args... )>::type>
    {
        using return_type = typename std::result_of<F( std::shared_ptr<PooledSQLDBCon<DBImpl>>, Args... )>::type;

        // Don't allow enqueueing jobs after stopping the pool.
        {
            std::unique_lock<std::mutex> xLock( this->xQueueMutex );
            if( bStop )
                throw std::runtime_error( "SQLDBConPool: You tried to enqueue on stopped thread pool." );
        } // end of scope for queue_mutex, so we unlock the mutex

        // std::placeholders::_1 and represents some future value (this value will be only known
        // during function definition.
        // TO DO: Use an auto lambda of C++ 17
        auto xTask = std::make_shared<std::packaged_task<return_type( std::shared_ptr<PooledSQLDBCon<DBImpl>> )>> //
            ( std::bind( std::forward<F>( func ), std::placeholders::_1, std::forward<Args>( args )... ) );

        // The future outcome of the task. The caller will be later blocked until this result will
        // be available.
        std::future<return_type> xFuture = xTask->get_future( );
        {
            // Mutual access to the task queue has to be synchronized.
            std::unique_lock<std::mutex> lock( xQueueMutex );

            // The task gets as input a shared pointer to a DBConnection for doing its job
            qTasks.push( [ xTask ]( std::shared_ptr<PooledSQLDBCon<DBImpl>> pDBCon ) { ( *xTask )( pDBCon ); } );
        } // end of scope of lock (lock gets released)
        // Inform some waiting consumer (worker) that we have a fresh task.
        this->xCondition.notify_one( );

        return xFuture;
    } // method enqueue


    // Usage example:
    //   dbpool.enqueue([&](std::shared_ptr<SQLDB<MySQLConDB>> pDBCon) { ... do something using the connection ... ;})
    // The statement delivers a future of type DBResponse that comprises the outcome of the operation.
    // Because threads are not allowed to terminate with still  a 'flying' exception, all exceptions must be catch in
    // the thread itself.
    // dbpool.
    //
}; // DB connection pool