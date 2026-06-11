Python API Reference
====================

Complete API reference extracted from docstrings.
For narrative examples see :doc:`quickstart`.

.. note::
   The ``rcspp._core`` module is the compiled C++ extension (pybind11).
   Its classes are re-exported through the higher-level Python wrappers below.

rcspp.graph
-----------

The main Python entry point — ``ResourceGraph`` and parameter / result types.

.. automodule:: rcspp.graph
   :members:
   :undoc-members:
   :show-inheritance:
   :special-members: __init__

rcspp.resource
--------------

All built-in extension, feasibility, dominance, and cost function descriptors.

.. automodule:: rcspp.resource
   :members:
   :undoc-members:
   :show-inheritance:

rcspp.pricing\_pool
-------------------

Column pool with activity tracking and shared-memory cross-process pricing.

.. automodule:: rcspp.pricing_pool
   :members:
   :undoc-members:
   :show-inheritance:

rcspp.logger
------------

Logging utilities.

.. automodule:: rcspp.logger
   :members:
   :undoc-members:
