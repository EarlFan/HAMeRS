#include "util/differences/DifferenceFirstOrder.hpp"

DifferenceFirstOrder::DifferenceFirstOrder(
    const std::string& object_name,
    const tbox::Dimension& dim):
        Difference(
            object_name,
            dim)
{
    d_num_difference_ghosts = hier::IntVector::getOne(d_dim);
}


/*
 * Compute the difference with the given cell data.
 */
void
DifferenceFirstOrder::computeDifference(
    boost::shared_ptr<pdat::CellData<double> >& difference,
    const boost::shared_ptr<pdat::CellData<double> >& cell_data,
    const hier::Box& domain,
    const int depth)
{
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
    TBOX_ASSERT(difference);
    TBOX_ASSERT(cell_data);
    TBOX_ASSERT(depth < cell_data->getDepth());
#endif
    
    // Declare a null pointer.
    boost::shared_ptr<pdat::CellData<double> > variable_local_mean;
    
    computeDifferenceWithVariableLocalMean(
        difference,
        variable_local_mean,
        cell_data,
        domain,
        depth);
}


/*
 * Compute the difference and the local mean of the given cell data.
 */
void
DifferenceFirstOrder::computeDifferenceWithVariableLocalMean(
    boost::shared_ptr<pdat::CellData<double> >& difference,
    boost::shared_ptr<pdat::CellData<double> >& variable_local_mean,
    const boost::shared_ptr<pdat::CellData<double> >& cell_data,
    const hier::Box& domain,
    const int depth)
{
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
    TBOX_ASSERT(difference);
    TBOX_ASSERT(cell_data);
    TBOX_ASSERT(depth < cell_data->getDepth());
#endif
    
    // Get the dimensions of box that covers the interior of patch.
    const hier::Box interior_box = cell_data->getBox();
    const hier::IntVector interior_dims = interior_box.numberCells();
    
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
    TBOX_ASSERT(difference->getBox().numberCells() == interior_dims);
#endif

    // Get the number of ghost cells of the cell data and difference data.
    const hier::IntVector num_ghosts_cell_data = cell_data->getGhostCellWidth();
    const hier::IntVector num_ghosts_difference = difference->getGhostCellWidth();
    
    // Get the dimensions of box that covers interior of patch plus ghost cells.
    const hier::Box ghost_box_cell_data = cell_data->getGhostBox();
    const hier::IntVector ghostcell_dims_cell_data = ghost_box_cell_data.numberCells();
    
    const hier::Box ghost_box_difference = difference->getGhostBox();
    const hier::IntVector ghostcell_dims_difference = ghost_box_difference.numberCells();
    
    /*
     * Get the local lower indices and number of cells in each direction of the domain.
     */
    
    hier::IntVector domain_lo(d_dim);
    hier::IntVector domain_dims(d_dim);
    
    if (domain.empty())
    {
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
        /*
         * Check potential failures.
         */
        
        if (num_ghosts_cell_data < d_num_difference_ghosts)
        {
            TBOX_ERROR(d_object_name
                << ": DifferenceFirstOrder::computeDifferenceWithVariableLocalMean()\n"
                << "The ghost cell width of cell data is smaller than required."
                << std::endl);
        }
#endif
        
        hier::IntVector num_ghosts_min(d_dim);
        
        num_ghosts_min = num_ghosts_cell_data - d_num_difference_ghosts;
        num_ghosts_min = hier::IntVector::min(num_ghosts_difference, num_ghosts_min);
        
        hier::Box ghost_box = interior_box;
        ghost_box.grow(num_ghosts_min);
        
        domain_lo = -num_ghosts_min;
        domain_dims = ghost_box.numberCells();
    }
    else
    {
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
        hier::Box shrinked_ghost_box_cell_data(ghost_box_cell_data);
        shrinked_ghost_box_cell_data.grow(-d_num_difference_ghosts);
        
        TBOX_ASSERT(shrinked_ghost_box_cell_data.contains(domain));
        TBOX_ASSERT(ghost_box_difference.contains(domain));
#endif        
        
        domain_lo = domain.lower() - interior_box.lower();
        domain_dims = domain.numberCells();
    }
    
    // Determine whether local mean is requred to be completed.
    bool compute_variable_local_mean = false;
    if (variable_local_mean)
    {
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
        TBOX_ASSERT(variable_local_mean->getBox().numberCells() == interior_dims);
#endif
        
        compute_variable_local_mean = true;
        
        const hier::IntVector num_ghosts_local_means = variable_local_mean->getGhostCellWidth();
        
#ifdef HAMERS_DEBUG_CHECK_ASSERTIONS
        if (num_ghosts_local_means != num_ghosts_difference)
        {
            TBOX_ERROR(d_object_name
                << ": DifferenceFirstOrder::computeDifferenceWithVariableLocalMean()\n"
                << "The number of ghost cells of variable local means doesn't match that of difference."
                << std::endl);
        }
#endif
    }
    
    // Get the pointer to the current depth component of the given cell data.
    double* f = cell_data->getPointer(depth);
    
    // Get the pointers to the difference and local mean.
    double* w = difference->getPointer(0);
    double* f_mean = nullptr;
    
    if (compute_variable_local_mean)
    {
        f_mean = variable_local_mean->getPointer(0);
    }
    
    if (d_dim == tbox::Dimension(1))
    {
        /*
         * Get the local lower index, numbers of cells in each dimension and numbers of ghost cells.
         */
        
        const int domain_lo_0 = domain_lo[0];
        const int domain_dim_0 = domain_dims[0];
        
        const int num_ghosts_0_cell_data = num_ghosts_cell_data[0];
        const int num_ghosts_0_difference = num_ghosts_difference[0];
        
#ifdef HAMERS_ENABLE_SIMD
        #pragma omp simd
#endif
        for (int i = domain_lo_0; i < domain_lo_0 + domain_dim_0; i++)
        {
            // Compute the linear indices.
            const int idx = i + num_ghosts_0_difference;
            
            const int idx_cell_data_x_L = i - 1 + num_ghosts_0_cell_data;
            const int idx_cell_data_x_R = i + 1 + num_ghosts_0_cell_data;
            
            w[idx] = fabs(double(1)/double(2)*(f[idx_cell_data_x_R] - f[idx_cell_data_x_L]));
        }
        
        if (compute_variable_local_mean)
        {
#ifdef HAMERS_ENABLE_SIMD
            #pragma omp simd
#endif
            for (int i = domain_lo_0; i < domain_lo_0 + domain_dim_0; i++)
            {
                // Compute the linear indices.
                const int idx = i + num_ghosts_0_difference;
                
                const int idx_cell_data_x_L = i - 1 + num_ghosts_0_cell_data;
                const int idx_cell_data_x_R = i + 1 + num_ghosts_0_cell_data;
                
                f_mean[idx] = double(1)/double(2)*(f[idx_cell_data_x_R] + f[idx_cell_data_x_L]);
            }
        }
    }
    else if (d_dim == tbox::Dimension(2))
    {
        /*
         * Get the local lower indices, numbers of cells in each dimension and numbers of ghost cells.
         */
        
        const int domain_lo_0 = domain_lo[0];
        const int domain_lo_1 = domain_lo[1];
        const int domain_dim_0 = domain_dims[0];
        const int domain_dim_1 = domain_dims[1];
        
        const int num_ghosts_0_cell_data = num_ghosts_cell_data[0];
        const int num_ghosts_1_cell_data = num_ghosts_cell_data[1];
        const int ghostcell_dim_0_cell_data = ghostcell_dims_cell_data[0];
        
        const int num_ghosts_0_difference = num_ghosts_difference[0];
        const int num_ghosts_1_difference = num_ghosts_difference[1];
        const int ghostcell_dim_0_difference = ghostcell_dims_difference[0];
        
        for (int j = domain_lo_1; j < domain_lo_1 + domain_dim_1; j++)
        {
#ifdef HAMERS_ENABLE_SIMD
            #pragma omp simd
#endif
            for (int i = domain_lo_0; i < domain_lo_0 + domain_dim_0; i++)
            {        
                // Compute the linear indices.
                const int idx = (i + num_ghosts_0_difference) +
                    (j + num_ghosts_1_difference)*ghostcell_dim_0_difference;
                
                const int idx_cell_data_x_L = (i - 1 + num_ghosts_0_cell_data) +
                    (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                
                const int idx_cell_data_x_R = (i + 1 + num_ghosts_0_cell_data) +
                    (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                
                const int idx_cell_data_y_B = (i + num_ghosts_0_cell_data) +
                    (j - 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                
                const int idx_cell_data_y_T = (i + num_ghosts_0_cell_data) +
                    (j + 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                
                double w_x = double(1)/double(2)*(f[idx_cell_data_x_R] - f[idx_cell_data_x_L]);
                double w_y = double(1)/double(2)*(f[idx_cell_data_y_T] - f[idx_cell_data_y_B]);
                
                w[idx] = sqrt(w_x*w_x + w_y*w_y);
            }
        }
        
        if (compute_variable_local_mean)
        {
            for (int j = domain_lo_1; j < domain_lo_1 + domain_dim_1; j++)
            {
#ifdef HAMERS_ENABLE_SIMD
                #pragma omp simd
#endif
                for (int i = domain_lo_0; i < domain_lo_0 + domain_dim_0; i++)
                {
                    // Compute the linear indices.
                    const int idx = (i + num_ghosts_0_difference) +
                        (j + num_ghosts_1_difference)*ghostcell_dim_0_difference;
                    
                    const int idx_cell_data_x_L = (i - 1 + num_ghosts_0_cell_data) +
                        (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                    
                    const int idx_cell_data_x_R = (i + 1 + num_ghosts_0_cell_data) +
                        (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                    
                    const int idx_cell_data_y_B = (i + num_ghosts_0_cell_data) +
                        (j - 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                    
                    const int idx_cell_data_y_T = (i + num_ghosts_0_cell_data) +
                        (j + 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data;
                    
                    double f_mean_x = double(1)/double(2)*(f[idx_cell_data_x_R] + f[idx_cell_data_x_L]);
                    double f_mean_y = double(1)/double(2)*(f[idx_cell_data_y_T] + f[idx_cell_data_y_B]);
                    
                    f_mean[idx] = sqrt(f_mean_x*f_mean_x + f_mean_y*f_mean_y);
                }
            }
        }
    }
    else if (d_dim == tbox::Dimension(3))
    {
        /*
         * Get the local lower indices, numbers of cells in each dimension and numbers of ghost cells.
         */
        
        const int domain_lo_0 = domain_lo[0];
        const int domain_lo_1 = domain_lo[1];
        const int domain_lo_2 = domain_lo[2];
        const int domain_dim_0 = domain_dims[0];
        const int domain_dim_1 = domain_dims[1];
        const int domain_dim_2 = domain_dims[2];
        
        const int num_ghosts_0_cell_data = num_ghosts_cell_data[0];
        const int num_ghosts_1_cell_data = num_ghosts_cell_data[1];
        const int num_ghosts_2_cell_data = num_ghosts_cell_data[2];
        const int ghostcell_dim_0_cell_data = ghostcell_dims_cell_data[0];
        const int ghostcell_dim_1_cell_data = ghostcell_dims_cell_data[1];
        
        const int num_ghosts_0_difference = num_ghosts_difference[0];
        const int num_ghosts_1_difference = num_ghosts_difference[1];
        const int num_ghosts_2_difference = num_ghosts_difference[2];
        const int ghostcell_dim_0_difference = ghostcell_dims_difference[0];
        const int ghostcell_dim_1_difference = ghostcell_dims_difference[1];
        
        for (int k = domain_lo_2; k < domain_lo_2 + domain_dim_2; k++)
        {
            for (int j = domain_lo_1; j < domain_lo_1 + domain_dim_1; j++)
            {
#ifdef HAMERS_ENABLE_SIMD
                #pragma omp simd
#endif
                for (int i = domain_lo_0; i < domain_lo_0 + domain_dim_0; i++)
                {
                    // Compute the linear indices.
                    const int idx = (i + num_ghosts_0_difference) +
                        (j + num_ghosts_1_difference)*ghostcell_dim_0_difference +
                        (k + num_ghosts_2_difference)*ghostcell_dim_0_difference*
                            ghostcell_dim_1_difference;
                    
                    const int idx_cell_data_x_L = (i - 1 + num_ghosts_0_cell_data) +
                        (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                        (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                            ghostcell_dim_1_cell_data;
                    
                    const int idx_cell_data_x_R = (i + 1 + num_ghosts_0_cell_data) +
                        (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                        (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                            ghostcell_dim_1_cell_data;
                    
                    const int idx_cell_data_y_B = (i + num_ghosts_0_cell_data) +
                        (j - 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                        (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                            ghostcell_dim_1_cell_data;
                    
                    const int idx_cell_data_y_T = (i + num_ghosts_0_cell_data) +
                        (j + 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                        (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                            ghostcell_dim_1_cell_data;
                    
                    const int idx_cell_data_z_B = (i + num_ghosts_0_cell_data) +
                        (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                        (k - 1 + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                            ghostcell_dim_1_cell_data;
                    
                    const int idx_cell_data_z_F = (i + num_ghosts_0_cell_data) +
                        (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                        (k + 1 + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                            ghostcell_dim_1_cell_data;
                    
                    double w_x = double(1)/double(2)*(f[idx_cell_data_x_R] - f[idx_cell_data_x_L]);
                    double w_y = double(1)/double(2)*(f[idx_cell_data_y_T] - f[idx_cell_data_y_B]);
                    double w_z = double(1)/double(2)*(f[idx_cell_data_z_F] - f[idx_cell_data_z_B]);
                    
                    w[idx] = sqrt(w_x*w_x + w_y*w_y + w_z*w_z);
                }
            }
        }
        
        if (compute_variable_local_mean)
        {
            for (int k = domain_lo_2; k < domain_lo_2 + domain_dim_2; k++)
            {
                for (int j = domain_lo_1; j < domain_lo_1 + domain_dim_1; j++)
                {
#ifdef HAMERS_ENABLE_SIMD
                    #pragma omp simd
#endif
                    for (int i = domain_lo_0; i < domain_lo_0 + domain_dim_0; i++)
                    {
                        // Compute the linear indices.
                        const int idx = (i + num_ghosts_0_difference) +
                            (j + num_ghosts_1_difference)*ghostcell_dim_0_difference +
                            (k + num_ghosts_2_difference)*ghostcell_dim_0_difference*
                                ghostcell_dim_1_difference;
                        
                        const int idx_cell_data_x_L = (i - 1 + num_ghosts_0_cell_data) +
                            (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                            (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                                ghostcell_dim_1_cell_data;
                        
                        const int idx_cell_data_x_R = (i + 1 + num_ghosts_0_cell_data) +
                            (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                            (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                                ghostcell_dim_1_cell_data;
                        
                        const int idx_cell_data_y_B = (i + num_ghosts_0_cell_data) +
                            (j - 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                            (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                                ghostcell_dim_1_cell_data;
                        
                        const int idx_cell_data_y_T = (i + num_ghosts_0_cell_data) +
                            (j + 1 + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                            (k + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                                ghostcell_dim_1_cell_data;
                        
                        const int idx_cell_data_z_B = (i + num_ghosts_0_cell_data) +
                            (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                            (k - 1 + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                                ghostcell_dim_1_cell_data;
                        
                        const int idx_cell_data_z_F = (i + num_ghosts_0_cell_data) +
                            (j + num_ghosts_1_cell_data)*ghostcell_dim_0_cell_data +
                            (k + 1 + num_ghosts_2_cell_data)*ghostcell_dim_0_cell_data*
                                ghostcell_dim_1_cell_data;
                        
                        double f_mean_x = double(1)/double(2)*(f[idx_cell_data_x_R] + f[idx_cell_data_x_L]);
                        double f_mean_y = double(1)/double(2)*(f[idx_cell_data_y_T] + f[idx_cell_data_y_B]);
                        double f_mean_z = double(1)/double(2)*(f[idx_cell_data_z_F] + f[idx_cell_data_z_B]);
                        
                        f_mean[idx] = sqrt(f_mean_x*f_mean_x + f_mean_y*f_mean_y + f_mean_z*f_mean_z);
                    }
                }
            }
        }
    }
}
