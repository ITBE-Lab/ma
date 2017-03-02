
from setuptools import setup, Extension
 
setup(
    name='LAuS_Aligner',
    version='0.0.1',
    description='An aligner for Nucleotide Sequences.',
    author='Markus Schmidt',
 
    author_email='markus.rainer.schmidt@gmail.com',
 
    url='http://itbe.hanyang.ac.kr/',
    include_package_data=True,
 
    zip_safe=False,
 
    #license='MIT', #TODO lookup correct license

    classifiers=[ 
        # How mature is this project? Common values are 
        #   3 - Alpha 
        #   4 - Beta 
        #   5 - Production/Stable 
        'Development Status :: 3 - Alpha', 


        # Indicate who your project is intended for 
        'Intended Audience :: Developers', 
        'Topic :: Scientific/Engineering :: Bio-Informatics', 

        # Pick your license as you wish (should match "license" above) 
        #'License :: OSI Approved :: MIT License', #TODO lookup correct license


        # Specify the Python versions you support here. In particular, ensure 
        # that you indicate whether you support Python 2, Python 3 or both. 
        'Programming Language :: Python :: 2', 
        'Programming Language :: Python :: 2.7', 
        'Programming Language :: Python :: 3', 
        'Programming Language :: Python :: 3.3', 
        'Programming Language :: Python :: 3.4', 
        'Programming Language :: Python :: 3.5', 
    ], 

    keywords='aligner nucleotide sequence',

    ext_modules=[
        Extension('LAuS_Aligner',
                 ['src/aligner.cpp',
                 'src/module.cpp',
                 'src/container.cpp',
                 'src/nucSeq.cpp',],
                  include_dirs=['inc'], # assuming your project include files are there
                  library_dirs=['/usr/lib'], # optional
                  libraries=['boost_python'], # those are the linked libs
                  extra_compile_args=['-std=c++11'] # some other compile args
                  ),
        ]
)