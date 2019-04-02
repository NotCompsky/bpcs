# Features

Allow other grid sizes
    Will be implemented without adding overhead. Necessitates templating to make the size selectable only at compile-time. Necessitates only reading/writing complete bytes to grids, which makes it advisable to use grid size MxN such that (M*N % 8 == 1) - for instance, (2m + 1)x(2m + 1) for any natural m. 

# Optimisations

Compile-time option to replace OpenCV use with specially-written CUDA
    Inputs:
        t (== complexity_threshold)
        w (== grid_width == grid_height)
        N_GRIDS_HRZTL
        N_GRIDS_VRTCL
        Individual MxN channel* (i.e. byteplane)
    Output:
        Array of (w+1) 2D matrices**, each of size N_GRIDS_HRZTL x N_GRIDS_VRTCL
            1:1 mapping from elements to corresponding bit grid
            First matrix specifies whether the grid complexity was over the threshold - i.e. whether to use the bytes from the following matrices corresponding to the same grid position
            Remaining matrices specify the byte values extracted from each grid
    
    * Using channels since we'd have to buffer a lot more data if we were using entire images. Not using bitplanes to further reduce buffering since converting to bitplanes would itself require buffering and extra operations.
    ** Matrices will be replaced by arrays, as contiguos data is desirable.

# Possible Optimisations

"Pre-calculate" complexities
    Rather than extract bitplane from a channel byteplane and iterating over its 9x9 grids, iterate over the byteplane itself, XORing the 9x9 grids (resulting in a 8x9 grid (M_1) and an 9x8 (M_2) grid), and iterating over each bit-depth of the result (only needing the sum over i of ((M_i >> n) & 1) to calculate the bit-depth's complexity).
    Would reduce XOR operations, and removes the need for the bitplane.

Multithreading:
    Reshape bitplane/byteplane from WxH to 8x8xN at once, 
    One thread per bitplane
        First bitplane sent to stdout as normal
        Other bitplanes have buffers set to 0.75*bitplane_capacity, which are then dumped to stdout
