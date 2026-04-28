# Review Checklist

Before finishing an AI-assisted change, check:

- [ ] Does the change preserve classroom safety?
- [ ] Does power-on behavior remain safe?
- [ ] Is active output untouched unless explicitly requested?
- [ ] Are unknown items marked `未確認` or `未検証`?
- [ ] Are unimplemented features not advertised?
- [ ] Are physical quantities named with units?
- [ ] Are UI / measurement / calibration / CSV / hardware control domains separated?
- [ ] Are user-visible behavior changes reflected in README, CHANGELOG, manuals, or operation logs as appropriate?
- [ ] If CSV format or value meaning changed, is backward compatibility or impact documented?
- [ ] Does the build command pass, or is the failure documented?
- [ ] Are calibration or measurement changes recorded in operations logs?
- [ ] Are student/school/private details absent?
- [ ] Is the diff limited to the requested task?
