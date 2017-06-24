/*---------------------------------------------------------------------------*\
    eulerSolver - A simple parallel first order Gas-dynamics solver
                  based on the OpenFOAM library
    Copyright (C) 2012, Pavanakumar Mohanamuraly

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include<fvCFD.H>
#include<UList.H>
#include "fluxSchemes.H"

int main(int argc, char *argv[])
{
   /// Register cmd line inputs
   Foam::argList::addOption( "mach", "Mach number");
   Foam::argList::addOption( "aoa", "Angle of attack" );
   Foam::argList::addOption( "cfl", "CFL number" );
   #include "setRootCase.H"
   /// Default values
   scalar M_inf, aoa, CFL;
   if( args.optionReadIfPresent( "mach", M_inf ) == false )
     Info << "Need a Mach number input " << Foam::FatalError;
   /// read AOA
   if( args.optionReadIfPresent( "aoa", aoa ) == false ) aoa = 0.0;
   /// Read CFL
   if( args.optionReadIfPresent( "cfl", CFL ) == false ) CFL = 0.8;
   #include "createTime.H"
   #include "createMesh.H"

   #include "createFields.H"
   #include "readFluxScheme.H"
      
   /// Time step loop
   /// Posts the non-blocking send/recv of fields
   long int iter = 0;
   while( runTime.loop() ) {

     /// Construct the fluxes at faces
     #include "constructFaceFlux.H"
     
     /// Sum-up face fluxes of all associated faces of a cell
     #include "sumFlux.H"
     
     /// Obtain boundary fluxes
     #include "boundaryFlux.H"

     Info << "Iteration = " << ++iter << "  ";

     /// State update
     #include "stateUpdateLTS.H"

      Info << " Max residue = " << rhoResidMax << endl;
     /// Solution output
     runTime.write();
   }
 
   return 0;
}


