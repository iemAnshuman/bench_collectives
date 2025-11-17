# Spack developement workflow

Create develop environment
`mkdir dev_env`
`spack env create -d dev_env/.`
`spacktivate dev_env/.`


Add HPX dependencies
`spack add hpx`
`spack install`

Start developement
`spack develop hpx`
`spack config blame develop`
`spack find -cv hpx`
`spack concretize -f`
`spack install`

Checkout collective branch
`cd hpx`
`git fetch origin pull/6668/head:collectives`

