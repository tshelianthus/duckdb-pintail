use duckdb::{Connection, Result, duckdb_entrypoint_c_api};
use std::error::Error;

#[duckdb_entrypoint_c_api]
pub unsafe fn extension_entrypoint(_con: Connection) -> Result<(), Box<dyn Error>> {
    // Phase 1 will register st_geohash and related functions here.
    Ok(())
}
