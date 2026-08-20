[group('release')]
release VERSION TAG='auto' REF='main':
    gh workflow run .github/workflows/publish.yml \
    	--ref "{{ REF }}" \
    	--field version="{{ VERSION }}" \
    	--field dist_tag="{{ TAG }}"

[group('release')]
preview-publish VERSION REF:
    just release "{{ VERSION }}" preview "{{ REF }}"

[group('release')]
release-preview VERSION REF:
    just preview-publish "{{ VERSION }}" "{{ REF }}"
