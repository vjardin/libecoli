..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2019 Olivier Matz <zer0@droids-corp.org>

Libecoli Documentation
======================

This is the documentation of *libecoli*. This library simplifies the
creation of interactive command line interfaces (*Ecoli* stands for
Extensible COmmand LIne).

What can it be used for?

- complex interactive cli interface in C (ex: a router cli)
- interactive cli for shell scripts
- application arguments parsing, natively supporting bash completion
- parsers

What are the main features?

- dynamic completion
- contextual help
- integrated with libedit, but can use any readline-like library
- modular: the cli behavior is defined through an assembly of basic
  nodes
- extensible: the user can write its own nodes to provide specific
  features
- C API
- YAML API and Shell API: used from shell scripts

Who use it?

- 6WIND_ for its Turbo Router cli

.. _6WIND: https://www.6wind.com

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   overview
   installation
   user_guide
   architecture
   contributing
