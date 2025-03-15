//
// Data Slicer Demo, Hacked up By G-Man of Group 42
//

main()
{
    int     base;
    int     data_in;
    int     current_state;
    long    counter;

    //
    // Lets use COM1 in this example.
    //
    base = COM1;

    //
    // Turn data slicer interface on.
    //
    outp(base+MCR,0x01);      // Set DTR 1 DSR 0 to power up interface

    //
    // Loop forever, polling the DSR to read the input state of the
    //  Data Slicer,  Press ctrl-break to exit loop.
    //
    while(1)
    {
        //
        // read while input is the same just inc counter
        //
        while(current_state == (data_in = inp(base+MSR))
            counter++;
        //
        // We have a change in state, Print out counter value
        //
        printf("counter = %lx\n",counter);
        //
        // Re-initalize state for next counter value
        //
        current_state=data_in;
        counter=0;
    }
}


