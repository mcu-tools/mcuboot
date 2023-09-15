- Fixed an issue with boot_serial repeats not being processed when
  output was sent, this would lead to a divergence of commands
  whereby later commands being sent would have the previous command
  output sent instead.
